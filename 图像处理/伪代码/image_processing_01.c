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
// 函数简介      在图像中从下向上搜索赛道左右边界的起始点
// 参数说明      image         待处理的只读图像数据指针 (const uint8_t *)
// 参数说明      p_left        用于存储左边界起点坐标的指针 (point *)
// 参数说明      p_right       用于存储右边界起点坐标的指针 (point *)
// 返回参数      bool          如果同时找到左右边界则返回true，否则返回false
// 备注信息      左边界定义为“黑到白”的跳变点，右边界定义为“白到黑”的跳变点。
// 备注信息      通过指针传递结果并使用指针访问图像数据以提升性能。
//-------------------------------------------------------------------------------------------------------------------
bool get_start_point(const uint8_t *image, point *p_left, point *p_right)
{
    int y, x;
    // 1. 从图像底部向上逐行扫描。
    //    y从IMAGE_H - 2开始，是为了跳过最底部的黑边，同时确保安全。
    //    y > 0 是为了避免扫描到最顶部的黑边。
    for (y = IMAGE_H - 2; y > 0; y--)
    {
        // 性能优化：计算当前行的起始地址，避免在内层循环中重复进行 y * IMAGE_W 的乘法运算。
        const uint8_t *row_ptr = image + y * IMAGE_W;
        
        // 重置标志位，确保每一行的搜索都是独立的。
        bool l_found = false;
        bool r_found = false;

        // 2. 特殊情况处理：检查赛道线是否紧贴左右图像边缘。
        //    这通常发生在摄像头视野不足，赛道部分移出画面时。
        //    检查 [1] 和 [2] 是因为 [0] 通常是预留的黑边。
        if (row_ptr[1] == IMAGE_WHITE && row_ptr[2] == IMAGE_WHITE)
        {
            l_found = true;
            p_left->x = 1;
            p_left->y = y;
        }
        // 同理，检查最右侧的边缘情况。
        if (row_ptr[IMAGE_W - 2] == IMAGE_WHITE && row_ptr[IMAGE_W - 3] == IMAGE_WHITE)
        {
            r_found = true;
            p_right->x = IMAGE_W - 2;
            p_right->y = y;
        }

        // 3. 在单行内进行常规扫描搜索。
        //    循环边界 x < (IMAGE_W - 3) 是为了防止在访问 row_ptr[x + 3] 时发生数组越界。
        //    例如，当 x = IMAGE_W - 4 时，x+3 = IMAGE_W - 1，这是合法的最大索引。
        for (x = 1; x < (IMAGE_W - 3); x++)
        {
            // 寻找左边界：寻找一个“黑,黑,白,白”的4像素模式。
            // 相比简单的“黑->白”跳变，这种模式对椒盐噪声等干扰有更强的抵抗力，更稳定。
            if (!l_found && row_ptr[x] == IMAGE_BLACK && row_ptr[x + 1] == IMAGE_BLACK
                             && row_ptr[x + 2] == IMAGE_WHITE && row_ptr[x + 3] == IMAGE_WHITE)
            {
                l_found = true;
                p_left->x = x; // 记录跳变前的位置
                p_left->y = y;
            }
            
            // 寻找右边界：寻找一个“白,白,黑,黑”的4像素模式。
            // 同样是为了增强抗干扰能力。
            if (!r_found && row_ptr[x] == IMAGE_WHITE && row_ptr[x + 1] == IMAGE_WHITE
                             && row_ptr[x + 2] == IMAGE_BLACK && row_ptr[x + 3] == IMAGE_BLACK)
            {
                r_found = true;
                p_right->x = x; // 记录跳变前的位置
                p_right->y = y;
            }

            // 检查是否在本行内已经同时找到了左右边界。
            if (l_found && r_found)
            {
                // 验证赛道宽度：增加一个宽度判断，可以有效滤除因噪点被误识别为赛道的情况。
                if ((p_right->x - p_left->x) > 10) // 例如，有效赛道宽度必须大于10个像素
                {
                    return true; // 找到有效起始行，函数成功返回
                }
                else
                {
                    // 如果宽度无效，可能是找到了一个假的右边界（例如一个噪点）。
                    // 我们通过 continue 继续循环，希望能找到一个更远、更真实的右边界。
                    continue;
                }
            }
        }
    }

    // 4. 如果完整遍历了所有行，仍然没有找到符合条件的起始点，则函数失败。
    return false; // 失败
}