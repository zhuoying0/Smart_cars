#include "stdint.h"
#include <stdbool.h>
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
#define MAX_EDGE_POINTS 240
/**
 * @brief 边缘跟踪器结构体
 * @details 封装了单条边缘跟踪所需的所有配置、状态和结果指针。
 * 使得代码更加模块化，避免了函数参数过多和全局变量依赖。
 */
typedef struct {
    // --- 配置参数 ---
    point           start_point;
    const grow* grow_table;

    // --- 结果存储 (数组内嵌) ---
    point           edge_array[MAX_EDGE_POINTS];      // <-- 修改点
    uint8_t         direction_array[MAX_EDGE_POINTS]; // <-- 修改点

    // --- 实时状态 ---
    point           current_point;
    uint16_t        count;
    bool            is_active;
} EdgeTracker;

/**
 * @brief   执行单步的边缘跟踪（优化核心）
 * @param   image           - 图像数据指针
 * @param   center_point    - 输入/输出: 当前边缘点的中心坐标，函数会更新它
 * @param   cont            - 输入/输出: 当前边缘点的计数器
 * @param   edge_array      - 存储找到的边缘点轨迹
 * @param   direction_array - 存储每一步的方向
 * @param   grow_table      - 方向查找表（左边用grow_l，右边用grow_r）
 * @return  bool            - 如果成功找到下一点则返回true，否则返回false
 *
 * @note    此函数实现了预测性搜索。它以上一步的方向为中心，在[-1, 6]的窗口(可调整，如[-2,6],[-3,4]）内搜索，
 * 优先检查前进方向，从而大幅提升平均搜索效率。
 */
static bool trace_single_step(const uint8_t* image, EdgeTracker* tracker)
{
    // 安全检查：如果已达到缓冲区上限，则停止跟踪
    if (tracker->count >= MAX_EDGE_POINTS - 1) {
        tracker->is_active = false;
        return false;
    }

    point a0, a1;
    // 从tracker中获取上一步的方向
    uint8_t prev_direction = tracker->direction_array[tracker->count];

    // 1. 预测性搜索：以上一次方向为中心，在-1到+6的范围内搜索8个方向，i=0时，探测的就是上一次的方向。
    // 这使得在直道或缓弯时，i=0或i=1就能命中，效率极高。
    for (int i = -1; i <= 6; i++) 
    {
        // 计算两个相邻的探测方向
        uint8_t dir0 = (prev_direction + i + 8) & 7; // +8确保索引为正
        uint8_t dir1 = (prev_direction + i + 1 + 8) & 7;

        // 计算两个相邻探测点a0和a1的坐标
        // 从tracker中获取当前点和方向表
        a0.x = tracker->current_point.x + tracker->grow_table[dir0].x;
        a0.y = tracker->current_point.y + tracker->grow_table[dir0].y;
        a1.x = tracker->current_point.x + tracker->grow_table[dir1].x;
        a1.y = tracker->current_point.y + tracker->grow_table[dir1].y;

        // 2. 检测到黑->白边缘特征
        if (image[a0.y * IMAGE_W + a0.x] < Global_threshold &&
            image[a1.y * IMAGE_W + a1.x] > Global_threshold) 
        {
            // 更新tracker的状态
            tracker->count++;
            uint8_t new_direction = dir1;
            tracker->direction_array[tracker->count] = new_direction;
            tracker->current_point.x += tracker->grow_table[new_direction].x;
            tracker->current_point.y += tracker->grow_table[new_direction].y;
            tracker->edge_array[tracker->count] = tracker->current_point;
            
            return true; // 成功找到下一点
        }
    }

    // 如果没找到，将跟踪器设为非活动状态
    tracker->is_active = false;
    return false; // 8个方向都没找到
}
/**
 * @brief 搜索图像中的左右边界线
 * @param image           - 经过二值化处理的图像数据
 * @param left_tracker    - 配置好的左边缘跟踪器
 * @param right_tracker   - 配置好的右边缘跟踪器
 * @param max_iterations  - 主循环的最大迭代次数，防止死循环
 */
void search_lr_line(const uint8_t* image, EdgeTracker* left_tracker, EdgeTracker* right_tracker, uint16_t max_iterations)
{
    // 1. 初始化两个跟踪器的状态
    // 函数内部负责完成所有状态的初始化
    left_tracker->count = 0;
    left_tracker->current_point = left_tracker->start_point; // <-- 从配置中读取
    left_tracker->edge_array[0] = left_tracker->start_point;
    left_tracker->direction_array[0] = 6;
    left_tracker->is_active = true;

    right_tracker->count = 0;
    right_tracker->current_point = right_tracker->start_point; // <-- 从配置中读取
    right_tracker->edge_array[0] = right_tracker->start_point;
    right_tracker->direction_array[0] = 6;
    right_tracker->is_active = true;

    // 2. 主循环：只要还有一个跟踪器在活动，就继续
    while (max_iterations-- > 0 && (left_tracker->is_active || right_tracker->is_active))
    {
        // 3. 同步调度策略：优先推进Y坐标更大(更靠后)的跟踪器
        if (left_tracker->is_active && right_tracker->is_active) {
            if (left_tracker->current_point.y >= right_tracker->current_point.y) {
                trace_single_step(image, left_tracker);
                trace_single_step(image, right_tracker);
            } else {
                trace_single_step(image, right_tracker);
                trace_single_step(image, left_tracker);
            }
        } else if (left_tracker->is_active) {
            trace_single_step(image, left_tracker);
        } else if (right_tracker->is_active) {
            trace_single_step(image, right_tracker);
        }

        // 4. 终止条件：边缘交汇
        if (left_tracker->is_active && right_tracker->is_active) {
            if (abs(left_tracker->current_point.x - right_tracker->current_point.x) < 5 &&
                abs(left_tracker->current_point.y - right_tracker->current_point.y) < 5) {
                break;
            }
        }
    }
}

EdgeTracker left_tracker, right_tracker;

void image_init()
{
    left_tracker.grow_table = grow_l;
    right_tracker.grow_table = grow_r;
}
void image_main_process() {
    // ... 图像预处理等步骤 ...
    point start_l, start_r;
    // 1. 获取起始点 (依然使用之前的get_start_point函数)
    if (!get_start_point(mt9v03x_image_copy[0], &start_l, &start_r)) {
        return; // 起始点未找到
    }
    // 2. image_main_process 的职责是“配置”跟踪器
    left_tracker.start_point = start_l;
    right_tracker.start_point = start_r;
    // 3. 执行跟踪
    search_lr_line(mt9v03x_image_copy[0], &left_tracker, &right_tracker, MAX_EDGE_POINTS);

    // 4. 跟踪完成，结果在left_tracker.edge_array 和 right_tracker.edge_array 数组中
    // 可以通过 left_tracker.count 和 right_tracker.count 获取找到的点的数量
    // ... 后续处理 ...
}
