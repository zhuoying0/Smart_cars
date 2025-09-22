#include "stdint.h"
#include <stdbool.h>
#include <string.h> // 为 memset 添加头文件
#include <stdlib.h> // 为 abs 添加头文件

#define IMAGE_W 188         // 图像处理宽度（像素）
#define IMAGE_H 120         // 图像处理高度（像素）
#define IMAGE_WHITE    255  //白色
#define IMAGE_BLACK    0    //黑色
#define MAX_EDGE_POINTS 240 //最大边缘点数
// 使用枚举类型来明确表示左右边缘，增强代码可读性和类型安全
typedef enum {
    EDGE_LEFT = 0,
    EDGE_RIGHT = 1
} EdgePolarity;

typedef struct {
    uint8_t x;  // X坐标
    uint8_t y;  // Y坐标
} point;

// 边缘生长方向结构体
typedef struct {
    int8_t x;  // x方向增量
    int8_t y;  // y方向增量
} grow;

/* 左边界搜索方向表（顺时针方向）*/
static grow grow_l[8] = {
    {0,-1},  // 0: 上移 ↑
    {1,-1},  // 1: 右上 ↗
    {1,0},   // 2: 右移 →
    {1,1},   // 3: 右下 ↘
    {0,1},   // 4: 下移 ↓
    {-1,1},  // 5: 左下 ↙
    {-1,0},  // 6: 左移 ←
    {-1,-1}  // 7: 左上 ↖
};

/* 右边界搜索方向表（逆时针方向）*/
static grow grow_r[8] = {
    {0,-1},  // 0: 上移 ↑
    {-1,-1}, // 1: 左上 ↖
    {-1,0},  // 2: 左移 ←
    {-1,1},  // 3: 左下 ↙
    {0,1},   // 4: 下移 ↓
    {1,1},   // 5: 右下 ↘
    {1,0},   // 6: 右移 →
    {1,-1}   // 7: 右上 ↗
};
typedef struct {
    // 原始循迹数据 (来自 search_lr_line)
    // --- 配置参数 ---
    point       start_point;
    const grow* grow_table;
    uint8_t     threshold;
    // --- 结果存储 (数组内嵌) ---
    point    raw_edge_points[MAX_EDGE_POINTS];
    uint8_t  raw_direction[MAX_EDGE_POINTS];
    uint16_t raw_points_count;
    // --- 实时状态 ---
    point           current_point;
    bool            is_active;

    // 中间数据 (来自 convert_edge_to_row_map)
    uint8_t mapped_edge[IMAGE_H];
    uint8_t mapped_edge_start_y;
    uint8_t mapped_edge_end_y;

    // 结果数据 (来自 extract_single_edge)
    point   filtered_edge[IMAGE_H];
    int     filtered_points_count;

    // 状态标志
    bool    breakpoint_flag;
    bool    is_found;

} EdgeTracker;

// 最高层的数据上下文，封装了左右两条边以及其他全局状态
typedef struct {
    EdgeTracker left_edge;
    EdgeTracker right_edge;
    
    // 其他可能需要的全局状态可以放在这里
    uint8_t final_distance;
} TrackContext;

//-------------------------------------------------------------------------------------------------------------------
// 函数简介      将离散的边缘点集转换为按行索引的“行地图”
// 参数说明      input         输入的原始边缘点数组
// 参数说明      size          输入数组的大小
// 参数说明      output        输出的行地图数组，大小为 IMAGE_H
// 返回参数      uint8_t       找到的边缘最高点的y坐标 (y值最小)
// 备注信息      此函数是数据预处理步骤，将(x,y)点集转换为更易于后续处理的 edge[y] = x 格式。
//-------------------------------------------------------------------------------------------------------------------
uint8_t convert_edge_to_row_map_first_point(const point *input, int size, uint8_t *output)
{
    // 1. 使用 memset 高效地将输出数组初始化为0。
    //    由于图像处理前已添加黑边，x=0不会是有效的边线点，因此0可作为“未写入”标记。
    memset(output, 0, IMAGE_H);

    // 初始化最小y值为最大高度，用于寻找边缘的最高点。
    uint8_t min_y = IMAGE_H;

    // 2. 遍历所有原始循迹点
    for (int i = 0; i < size; i++)
    {
        uint8_t current_y = input[i].y;

        // 核心逻辑：只有当该行在行地图中还未被赋值时，才进行写入。
        // 因为原始循迹点可能是无序或重复的，此操作确保每行只记录一个x值，简化了数据。
        if (output[current_y] == 0)
        {
            output[current_y] = input[i].x;
            // 持续更新找到的最高点的y坐标。
            min_y = current_y < min_y ? current_y : min_y;
        }
        
    }
    return min_y; // 返回边缘的最高点
}

// 定义边缘提取算法的参数，这些参数决定了算法的灵敏度和鲁棒性
#define MIN_VALID_SEGMENT_LENGTH   6   // 一个边缘段被认为是有效的最小连续行数
#define MAX_EDGE_HORIZONTAL_JUMP   8   // 连续两行之间允许的最大水平像素跳变
#define INVALID_EDGE_LEFT_X        1   // 左侧无效边缘的X坐标 (黑边框)
#define INVALID_EDGE_RIGHT_X       (IMAGE_W - 2) // 右侧无效边缘的X坐标

// 定义函数内部使用的状态机状态
typedef enum {
    STATE_SEARCHING, // 状态：正在从下往上寻找有效线段的起点
    STATE_TRACKING   // 状态：已找到起点，正在跟踪一个有效的线段
} EdgeExtractionState;

//-------------------------------------------------------------------------------------------------------------------
// 函数简介      从“行地图”中提取最长、最连续的有效边缘段
// 参数说明      tracker       指向边缘跟踪器的指针，函数会读取其中的mapped_edge并写入filtered_edge
// 参数说明      polarity      指示当前处理的是左边缘还是右边缘
// 备注信息      这是滤波算法的核心。它使用一个状态机来识别并提取主边缘，过滤掉噪声和短的无效段。
//-------------------------------------------------------------------------------------------------------------------
static void extract_single_edge(EdgeTracker *tracker, EdgePolarity polarity)
{
    // 从 tracker 结构体中获取所需的数据指针和参数，简化后续代码
    uint8_t *most_edge = tracker->mapped_edge;       // 输入：行地图
    point *reality_edge = tracker->filtered_edge;    // 输出：滤波后的边缘点
    uint8_t start_y = tracker->mapped_edge_start_y;  // 起始扫描行
    
    // 根据是左边缘还是右边缘，确定无效区域的X坐标
    const uint8_t invalid_edge_x = (polarity == 0) ? INVALID_EDGE_LEFT_X : INVALID_EDGE_RIGHT_X;
    // 扫描的上限（最高点）
    const uint8_t upper_bound_y = tracker->mapped_edge_end_y;

    // 重置输出结果和状态标志
    tracker->filtered_points_count = 0;
    tracker->breakpoint_flag = false;

    // 初始化状态机
    EdgeExtractionState state = STATE_SEARCHING;
    int count = 0; // 用于记录当前有效线段的长度

    // 核心算法：从起始行(图像底部附近)向上扫描，直到边缘的最高点
    for (int y = start_y; y > upper_bound_y; y--)
    {
        const uint8_t current_x = most_edge[y];
        
        switch (state)
        {
            // 搜索状态：寻找第一个不是无效点的像素，作为线段的起点
            case STATE_SEARCHING:
                if (current_x != invalid_edge_x)
                {
                    // 找到起点，切换到跟踪状态
                    state = STATE_TRACKING;
                    // 关键技巧：将y加1，使循环在下一次迭代时重新处理当前行。
                    // 这样，找到的第一个点就会被作为有效线段的第一个点记录下来。
                    y++;
                    count = 0;
                }
                break;

            // 跟踪状态：持续记录连续的边缘点
            case STATE_TRACKING:
            {
                // 判断当前点是否构成“断点”
                bool is_discontinuous = (current_x == invalid_edge_x);
                // 条件1：当前点本身是无效点
                if (!is_discontinuous && count > 0)
                {
                    int last_x = reality_edge[count - 1].x;
                    // 条件2：当前点与上一个点水平距离过大（跳变）
                    if (abs(current_x - last_x) > MAX_EDGE_HORIZONTAL_JUMP)
                    {
                        is_discontinuous = true;
                        // 修改点：直接写入 tracker 的标志位
                        tracker->breakpoint_flag = true;  // 记录发生了跳变
                    }
                }
                // 如果发生了断点
                if (is_discontinuous)
                {
                    // 检查已跟踪的线段长度是否足够长
                    if (count >= MIN_VALID_SEGMENT_LENGTH)
                    {
                        // 如果足够长，说明我们找到了一个有效的主边缘段，任务完成。
                        goto extraction_finished;
                    }
                    else
                    {
                        // 如果线段太短，则认为是噪声，抛弃它
                        state = STATE_SEARCHING; // 回到搜索状态，寻找下一个可能的起点
                        count = 0;
                    }
                }
                else // 如果没有断点，说明边缘是连续的
                {
                    // 将当前点记录到结果数组中
                    reality_edge[count].x = current_x;
                    reality_edge[count].y = (uint8_t)y;
                    count++;
                }
                break;
            }
        }
    }
extraction_finished:
    // 循环结束后，对找到的最后一段进行有效性检查
    if (count < MIN_VALID_SEGMENT_LENGTH)
    {
        count = 0;
        tracker->is_found = false;
    }
    else
    {
        tracker->is_found = true; // 最终找到的线段是有效的
    }
    
    // 将最终统计到的有效点数存回 tracker 结构体
    tracker->filtered_points_count = count;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介      调度左右两条边的边缘提取
//-------------------------------------------------------------------------------------------------------------------
void extract_reality_edge(TrackContext *context) {
    // 处理左边缘
    extract_single_edge(&context->left_edge, EDGE_LEFT);

    // 处理右边缘
    extract_single_edge(&context->right_edge, EDGE_RIGHT);
}
// 作用：作为数据准备和处理流程的总调度函数。
// 它将 search_lr_line 产生的全局变量数据，安全地迁移到 TrackContext 结构体中，
// 然后调用新的、模块化的函数进行处理。
//-------------------------------------------------------------------------------------------------------------------
// 函数简介      边缘处理流程的总调度函数
// 备注信息      该函数串联了“格式转换”和“边缘提纯”两个主要步骤，并计算最终的有效循迹距离。
//-------------------------------------------------------------------------------------------------------------------
void extract_and_filter_edges(TrackContext *context)
{
    // --- 1. 格式转换：调用工具函数，将离散点集转换为按行索引的数组 ---
    context->left_edge.mapped_edge_end_y = convert_edge_to_row_map_first_point(context->left_edge.raw_edge_points, 
                                                                               context->left_edge.raw_points_count, 
                                                                               context->left_edge.mapped_edge);
                                        
    context->right_edge.mapped_edge_end_y = convert_edge_to_row_map_first_point(context->right_edge.raw_edge_points, 
                                                                                context->right_edge.raw_points_count, 
                                                                                context->right_edge.mapped_edge);

    // --- 2. 执行核心边缘提纯 ---
    extract_reality_edge(context);

    // --- 3. 计算有效循迹距离 ---
    // 有效距离取决于左右两边中，走得更“远”（y坐标更小）的那一边。
    uint8_t left_end_y = context->left_edge.mapped_edge_end_y;
    uint8_t right_end_y = context->right_edge.mapped_edge_end_y;

    if(left_end_y <= right_end_y && left_end_y > 0)
    {
        context->final_distance = IMAGE_H - left_end_y;
    }
    else if (right_end_y < left_end_y && right_end_y > 0)
    {
        context->final_distance = IMAGE_H - right_end_y;
    }
}
//-------------------------------------------------------------------------------------------------------------------
// 函数简介      图像处理主流程
// 参数说明      context       指向TrackContext的指针，用于管理整个处理过程的数据
// 备注信息      这是一个高层调度函数，串联了从“获取起点”到“执行循迹”的完整流程。
//-------------------------------------------------------------------------------------------------------------------
void image_main_process(TrackContext *context) {

    // --- 1. 配置阶段 ---
    // 设置二值化阈值等参数
    context->left_edge.threshold = 128;  // 示例阈值
    context->right_edge.threshold = 128; // 可以为左右设置不同阈值
    // 调用函数查找循迹的起始点
    if (!get_start_point(mt9v03x_image_copy[0], &context->left_edge.start_point, &context->right_edge.start_point)) {
        return; // 如果找不到起始点，则直接退出本次处理
    }
    // --- 2. 执行阶段 ---
    // 传入配置好的上下文，执行双边循迹算法
    search_line(mt9v03x_image_copy[0], &context->left_edge, &context->right_edge, MAX_EDGE_POINTS * 2);

    // --- 3. 结果处理阶段 ---
    extract_and_filter_edges(context);
}