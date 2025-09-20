#include "stdint.h"
#include <stdbool.h>
#define IMAGE_W 188         // 图像处理宽度（像素）
#define IMAGE_H 120         // 图像处理高度（像素）
#define IMAGE_WHITE    255
#define IMAGE_BLACK    0
#define MAX_EDGE_POINTS 240
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
//-------------------------------------------------------------------------------------------------------------------
// 函数简介      边缘跟踪器结构体
// 备注信息      封装了单条边缘跟踪所需的所有配置、状态和结果指针。
// 备注信息      使得代码更加模块化，避免了函数参数过多和全局变量依赖。
//-------------------------------------------------------------------------------------------------------------------
// 用于封装单条边缘所有相关数据的结构体
typedef struct {
    // 原始循迹数据 (来自 search_lr_line)
    // --- 配置参数 ---
    point       start_point;
    const grow* grow_table;
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
    // 1. 从图像底部向上逐行扫描 (y > 0 是为了避免访问黑边外的区域)
    for (y = IMAGE_H - 2; y > 0; y--)
    {
        // 获取当前扫描行的起始内存地址，为指针优化做准备
        const uint8_t *row_ptr = image + y * IMAGE_W;
        
        // 重置当前行的查找标志
        bool l_found = false;
        bool r_found = false;

        // 2. 处理赛道线紧贴图像边缘的特殊情况
        if (row_ptr[1] == IMAGE_WHITE && row_ptr[2] == IMAGE_WHITE)
        {
            l_found = true;
            p_left->x = 1;
            p_left->y = y;
        }
        if (row_ptr[IMAGE_W - 2] == IMAGE_WHITE && row_ptr[IMAGE_W - 3] == IMAGE_WHITE)
        {
            r_found = true;
            p_right->x = IMAGE_W - 2;
            p_right->y = y;
        }

        // 3. 在行内从左到右进行常规搜索
        // 使用 row_ptr[x] 代替 image[y * IMAGE_W + x]，避免了重复的乘法运算
        for (x = 1; x < (IMAGE_W - 2); x++)
        {
            // 如果左边界还没找到，则寻找 黑黑->白白 跳变
            if (!l_found && row_ptr[x] == IMAGE_BLACK && row_ptr[x + 1] == IMAGE_BLACK
                             && row_ptr[x + 2] == IMAGE_WHITE && row_ptr[x + 3] == IMAGE_WHITE)
            {
                l_found = true;
                p_left->x = x;
                p_left->y = y;
            }
            
            // 如果右边界还没找到，则寻找 白白->黑黑 跳变
            if (!r_found && row_ptr[x] == IMAGE_WHITE && row_ptr[x + 1] == IMAGE_WHITE
                             && row_ptr[x + 2] == IMAGE_BLACK && row_ptr[x + 3] == IMAGE_BLACK)
            {
                r_found = true;
                p_right->x = x;
                p_right->y = y;
            }

            // 如果在本行内左右边界都已找到，则立即跳出内层循环
            if (l_found && r_found)
            {
                // 可选：增加一个宽度判断，防止左右边界点离得太近
                if ((p_right->x - p_left->x) > 10) // 例如最小宽度为10个像素
                {
                    return true; // 成功
                }
                else
                {
                    continue;
                }
            }
        }
    }

    // 4. 如果遍历完所有行都没有同时找到左右边界，则返回失败
    return false; // 失败
}