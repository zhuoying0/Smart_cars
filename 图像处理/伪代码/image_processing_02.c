#include "stdint.h"
#include <stdbool.h>
#define IMAGE_W 188         // 图像处理宽度（像素）
#define IMAGE_H 120         // 图像处理高度（像素）
#define IMAGE_WHITE    255  //白色
#define IMAGE_BLACK    0    //黑色
#define MAX_EDGE_POINTS 240 //最大边缘点数
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
    //int global_error;
    uint8_t final_distance;
} TrackContext;
//-------------------------------------------------------------------------------------------------------------------
// 函数简介      单步边缘跟踪
// 参数说明      image         图像数据指针
// 参数说明      tracker       需要进行单步推进的边缘跟踪器
// 返回参数      bool          成功找到下一点则返回true，否则返回false
// 备注信息      这是循迹算法的核心，它从当前点出发，根据上一方向预测性地搜索下一个边缘点。
//-------------------------------------------------------------------------------------------------------------------
static bool trace_single_step(const uint8_t* image, EdgeTracker* tracker)
{
    // 安全检查：如果存储点的缓冲区已满，则强制停止跟踪，防止数组越界。
    if (tracker->raw_points_count >= MAX_EDGE_POINTS - 1) {
        tracker->is_active = false;
        return false;
    }

    point a0, a1;
    // 获取上一步的前进方向，作为本次搜索的基准。
    uint8_t prev_direction = tracker->raw_direction[tracker->raw_points_count];

    // 1. 预测性搜索：以 `prev_direction` 为中心，在[-1, 6]的范围内进行8次方向探测。
    //    这种策略基于“赛道线是连续的”这一先验知识。在直道或缓弯，下一个点很可能就在上一个方向（i=0）附近。
    //    这样可以极大地提高搜索效率，避免了盲目的全局搜索。
    for (int i = -1; i <= 6; i++) 
    {
        // 技巧：通过 `& 7` (等效于 % 8) 实现方向的循环计算，确保索引始终在 [0, 7] 范围内。
        // `+8` 是为了防止 `prev_direction + i` 出现负数。
        uint8_t dir0 = (prev_direction + i + 8) & 7;
        uint8_t dir1 = (prev_direction + i + 1 + 8) & 7;

        // 根据探测方向和方向增量表(grow_table)，计算出相邻的两个探测点 a0 和 a1 的坐标。
        a0.x = tracker->current_point.x + tracker->grow_table[dir0].x;
        a0.y = tracker->current_point.y + tracker->grow_table[dir0].y;
        a1.x = tracker->current_point.x + tracker->grow_table[dir1].x;
        a1.y = tracker->current_point.y + tracker->grow_table[dir1].y;

        // 2. 边缘特征检测：寻找从“黑”到“白”的跳变。
        //    根据左右边界 grow_table 的不同设计，这个条件可以通用地表示“从赛道内侧到赛道外侧”的跳变。
        if (image[a0.y * IMAGE_W + a0.x] < tracker->threshold &&
            image[a1.y * IMAGE_W + a1.x] > tracker->threshold) 
        {
            // 3. 状态更新：如果找到下一点，则更新跟踪器的所有状态。
            tracker->raw_points_count++; // 找到的点数量加一
            uint8_t new_direction = dir1; // 新的前进方向
            tracker->raw_direction[tracker->raw_points_count] = new_direction; // 存储新方向
            // 根据新方向更新当前点坐标
            tracker->current_point.x += tracker->grow_table[new_direction].x;
            tracker->current_point.y += tracker->grow_table[new_direction].y;
            tracker->raw_edge_points[tracker->raw_points_count] = tracker->current_point; // 存储新点
            
            return true; // 成功找到，立即返回
        }
    }

    // 如果遍历完8个方向都未找到符合条件的点，说明边缘中断。
    tracker->is_active = false;
    return false; // 8个方向都没找到
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介      执行左右双边循迹
// 参数说明      image         图像数据指针
// 参数说明      left_tracker  左边缘跟踪器
// 参数说明      right_tracker 右边缘跟踪器
// 参数说明      max_iterations 最大迭代次数，防止死循环
// 备注信息      此函数负责初始化并调度左右两个跟踪器，直到循迹完成或达到终止条件。
//-------------------------------------------------------------------------------------------------------------------
void search_line(const uint8_t* image, EdgeTracker* left_tracker, EdgeTracker* right_tracker, uint16_t max_iterations)
{
    // 1. 初始化两个跟踪器的状态，为一次全新的循迹做准备。
    left_tracker->raw_points_count = 0;
    left_tracker->current_point = left_tracker->start_point; // 从配置的起始点开始
    left_tracker->raw_edge_points[0] = left_tracker->start_point;
    left_tracker->raw_direction[0] = 0; // 初始方向统一为向上
    left_tracker->is_active = true; // 激活跟踪器

    right_tracker->raw_points_count = 0;
    right_tracker->current_point = right_tracker->start_point; // 从配置的起始点开始
    right_tracker->raw_edge_points[0] = right_tracker->start_point;
    right_tracker->raw_direction[0] = 0; // 初始方向统一为向上
    right_tracker->is_active = true; // 激活跟踪器

    // 2. 主循环：只要迭代次数未耗尽，且至少还有一个跟踪器在活动状态，就继续循环。
    while (max_iterations-- > 0 && (left_tracker->is_active || right_tracker->is_active))
    {
        // 3. 同步调度策略：目的是让左右两条线的跟踪进度保持基本一致。
        //    优先推进Y坐标更大（即更靠后、更接近图像底部）的跟踪器，让它“追赶”上另一个。
        //    这样可以防止因一边是直道另一边是弯道，导致一个跟踪器远远领先另一个，便于后续处理。
        if (left_tracker->is_active && right_tracker->is_active) {
            if (left_tracker->current_point.y >= right_tracker->current_point.y) {
                trace_single_step(image, left_tracker); // 左边在后，只推进左边
            } else {
                trace_single_step(image, right_tracker); // 右边在后，只推进右边
            }
        } else if (left_tracker->is_active) { // 如果只有左边在活动，则只推进左边
            trace_single_step(image, left_tracker);
        } else if (right_tracker->is_active) { // 如果只有右边在活动，则只推进右边
            trace_single_step(image, right_tracker);
        }

        // 4. 终止条件：当左右边缘的当前点非常接近时，认为它们已经交汇。
        if (left_tracker->is_active && right_tracker->is_active) {
            if (abs(left_tracker->current_point.x - right_tracker->current_point.x) < 5 &&
                abs(left_tracker->current_point.y - right_tracker->current_point.y) < 5) {
                break;
            }
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------
// 函数简介      初始化循迹上下文
// 参数说明      context       指向TrackContext的指针
// 备注信息      负责完成一些一次性的配置，如关联方向表。
//-------------------------------------------------------------------------------------------------------------------
void image_init(TrackContext *context)
{
    // 为左右边缘跟踪器关联不同的搜索方向表
    context->left_edge.grow_table = grow_l;
    context->right_edge.grow_table = grow_r;
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
    // 循迹完成后，原始的边缘点数据存储在 context->left_edge.raw_edge_points 等数组中
    // 可以通过 context->left_edge.raw_points_count 获取找到的点的数量
    // 接下来可以调用其他函数对这些原始数据进行滤波、拟合等后续处理
}