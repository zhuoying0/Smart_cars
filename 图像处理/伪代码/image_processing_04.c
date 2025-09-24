#include "stdint.h"
#include <stdbool.h>
#include <string.h> // 为 memset 添加头文件
#include <stdlib.h> // 为 abs 添加头文件
#include <math.h>// 用于 powf 和 sqrtf

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
// 用于浮点数计算的点结构体，避免精度损失
typedef struct {
    float x;
    float y;
} point_f;
// 用于存储三阶贝塞尔曲线四个控制点的结构体
typedef struct {
    point_f p0, p1, p2, p3;
} CubicBezier;
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
    
    // 新增的弯心成员
    point   turn_center;
    int16_t max_deviation;
    bool    is_turn_found;
} EdgeTracker;

// 最高层的数据上下文，封装了左右两条边以及其他全局状态
typedef struct {
    EdgeTracker left_edge;
    EdgeTracker right_edge;
    // 贝塞尔曲线结果
    CubicBezier left_bezier;
    CubicBezier right_bezier;
    bool left_bezier_found;
    bool right_bezier_found;
    // 其他可能需要的全局状态可以放在这里
    uint8_t final_distance;
} TrackContext;
// 辅助函数：计算两点之间距离的平方
static float distance_sq(point_f p1, point_f p2) {
    return (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y);
}
//-------------------------------------------------------------------------------------------------------------------
// 函数简介      将一系列离散点拟合为一条三阶贝塞尔曲线
// 参数说明      points        输入的离散点数组 (类型为原始的 point)
// 参数说明      count         输入的点的数量
// 返回参数      CubicBezier   计算得到的贝塞尔曲线（包含四个控制点 P0, P1, P2, P3）
// 备注信息      核心思想是：
//               1. P0 和 P3 直接取点集的首尾点。
//               2. 通过最小二乘法，求解出最优的中间控制点 P1 和 P2。
//-------------------------------------------------------------------------------------------------------------------
CubicBezier fit_bezier_curve(const point* points, int count) {
    CubicBezier bezier;

    // 安全检查：至少需要2个点才能定义一条线
    if (count < 2) {
        // 返回一个无效的曲线 (所有点都为0)
        bezier.p0 = bezier.p1 = bezier.p2 = bezier.p3 = (point_f){0, 0};
        return bezier;
    }

    // --- 1. 将原始点转换为浮点数点，并确定 P0 和 P3 ---
    point_f* points_f = (point_f*)malloc(count * sizeof(point_f));
    for (int i = 0; i < count; i++) {
        points_f[i] = (point_f){(float)points[i].x, (float)points[i].y};
    }
    bezier.p0 = points_f[0];
    bezier.p3 = points_f[count - 1];


    // --- 2. 参数化：为每个数据点分配一个 t 值 [0, 1] ---
    // 我们使用“弦长参数化”，这通常能得到最好的结果。
    // t 值与该点到起点的累积距离成正比。
    float* t_values = (float*)malloc(count * sizeof(float));
    t_values[0] = 0.0f;
    float total_length = 0;
    for (int i = 1; i < count; i++) {
        total_length += sqrtf(distance_sq(points_f[i], points_f[i-1]));
        t_values[i] = total_length;
    }
    // 归一化 t 值
    for (int i = 1; i < count; i++) {
        t_values[i] /= total_length;
    }

    // --- 3. 构建最小二乘法矩阵 ---
    // 我们需要求解 P1 和 P2。这可以表示为一个 2x2 的线性方程组：
    // C[0][0]*P1 + C[0][1]*P2 = X[0]
    // C[1][0]*P1 + C[1][1]*P2 = X[1]
    
    float C[2][2] = {{0, 0}, {0, 0}};
    point_f X[2] = {{0, 0}, {0, 0}};

    for (int i = 0; i < count; i++) {
        float t = t_values[i];
        float t_inv = 1.0f - t;
        
        // 贝塞尔基函数
        float b0 = t_inv * t_inv * t_inv;
        float b1 = 3.0f * t * t_inv * t_inv;
        float b2 = 3.0f * t * t * t_inv;
        float b3 = t * t * t;

        // 计算矩阵 C (常数项)
        C[0][0] += b1 * b1;
        C[0][1] += b1 * b2;
        // C[1][0] = C[0][1]
        C[1][1] += b2 * b2;
        
        // 计算矩阵 X (目标向量)
        point_f d_prime = {
            points_f[i].x - (b0 * bezier.p0.x + b3 * bezier.p3.x),
            points_f[i].y - (b0 * bezier.p0.y + b3 * bezier.p3.y)
        };
        X[0].x += b1 * d_prime.x;
        X[0].y += b1 * d_prime.y;
        X[1].x += b2 * d_prime.x;
        X[1].y += b2 * d_prime.y;
    }
    C[1][0] = C[0][1];

    // --- 4. 求解 2x2 线性方程组 ---
    float det_C = C[0][0] * C[1][1] - C[0][1] * C[1][0];
    if (fabsf(det_C) > 1e-6) { // 如果行列式不为0
        float det_C_inv = 1.0f / det_C;
        
        // 使用克莱姆法则或逆矩阵求解 P1
        bezier.p1.x = det_C_inv * (X[0].x * C[1][1] - X[1].x * C[0][1]);
        bezier.p1.y = det_C_inv * (X[0].y * C[1][1] - X[1].y * C[0][1]);
        
        // 求解 P2
        bezier.p2.x = det_C_inv * (X[1].x * C[0][0] - X[0].x * C[1][0]);
        bezier.p2.y = det_C_inv * (X[1].y * C[0][0] - X[0].y * C[1][0]);
        
    } else { // 行列式为0或非常小，说明所有点可能共线
        // 使用一种简单的启发式方法来处理共线情况
        bezier.p1 = (point_f){bezier.p0.x * (2.0f/3.0f) + bezier.p3.x * (1.0f/3.0f), 
                              bezier.p0.y * (2.0f/3.0f) + bezier.p3.y * (1.0f/3.0f)};
        bezier.p2 = (point_f){bezier.p0.x * (1.0f/3.0f) + bezier.p3.x * (2.0f/3.0f), 
                              bezier.p0.y * (1.0f/3.0f) + bezier.p3.y * (2.0f/3.0f)};
    }
    
    // --- 5. 释放内存并返回结果 ---
    free(points_f);
    free(t_values);
    
    return bezier;
}//-------------------------------------------------------------------------------------------------------------------
// 函数简介      将一系列离散点拟合为一条三阶贝塞尔曲线
// 参数说明      points        输入的离散点数组 (类型为原始的 point)
// 参数说明      count         输入的点的数量
// 返回参数      CubicBezier   计算得到的贝塞尔曲线（包含四个控制点 P0, P1, P2, P3）
// 备注信息      核心思想是：
//               1. P0 和 P3 直接取点集的首尾点。
//               2. 通过最小二乘法，求解出最优的中间控制点 P1 和 P2。
//-------------------------------------------------------------------------------------------------------------------
CubicBezier fit_bezier_curve(const point* points, int count) {
    CubicBezier bezier;

    // 安全检查：至少需要2个点才能定义一条线
    if (count < 2) {
        // 返回一个无效的曲线 (所有点都为0)
        bezier.p0 = bezier.p1 = bezier.p2 = bezier.p3 = (point_f){0, 0};
        return bezier;
    }

    // --- 1. 将原始点转换为浮点数点，并确定 P0 和 P3 ---
    point_f* points_f = (point_f*)malloc(count * sizeof(point_f));
    for (int i = 0; i < count; i++) {
        points_f[i] = (point_f){(float)points[i].x, (float)points[i].y};
    }
    bezier.p0 = points_f[0];
    bezier.p3 = points_f[count - 1];


    // --- 2. 参数化：为每个数据点分配一个 t 值 [0, 1] ---
    // 我们使用“弦长参数化”，这通常能得到最好的结果。
    // t 值与该点到起点的累积距离成正比。
    float* t_values = (float*)malloc(count * sizeof(float));
    t_values[0] = 0.0f;
    float total_length = 0;
    for (int i = 1; i < count; i++) {
        total_length += sqrtf(distance_sq(points_f[i], points_f[i-1]));
        t_values[i] = total_length;
    }
    // 归一化 t 值
    for (int i = 1; i < count; i++) {
        t_values[i] /= total_length;
    }

    // --- 3. 构建最小二乘法矩阵 ---
    // 我们需要求解 P1 和 P2。这可以表示为一个 2x2 的线性方程组：
    // C[0][0]*P1 + C[0][1]*P2 = X[0]
    // C[1][0]*P1 + C[1][1]*P2 = X[1]
    
    float C[2][2] = {{0, 0}, {0, 0}};
    point_f X[2] = {{0, 0}, {0, 0}};

    for (int i = 0; i < count; i++) {
        float t = t_values[i];
        float t_inv = 1.0f - t;
        
        // 贝塞尔基函数
        float b0 = t_inv * t_inv * t_inv;
        float b1 = 3.0f * t * t_inv * t_inv;
        float b2 = 3.0f * t * t * t_inv;
        float b3 = t * t * t;

        // 计算矩阵 C (常数项)
        C[0][0] += b1 * b1;
        C[0][1] += b1 * b2;
        // C[1][0] = C[0][1]
        C[1][1] += b2 * b2;
        
        // 计算矩阵 X (目标向量)
        point_f d_prime = {
            points_f[i].x - (b0 * bezier.p0.x + b3 * bezier.p3.x),
            points_f[i].y - (b0 * bezier.p0.y + b3 * bezier.p3.y)
        };
        X[0].x += b1 * d_prime.x;
        X[0].y += b1 * d_prime.y;
        X[1].x += b2 * d_prime.x;
        X[1].y += b2 * d_prime.y;
    }
    C[1][0] = C[0][1];

    // --- 4. 求解 2x2 线性方程组 ---
    float det_C = C[0][0] * C[1][1] - C[0][1] * C[1][0];
    if (fabsf(det_C) > 1e-6) { // 如果行列式不为0
        float det_C_inv = 1.0f / det_C;
        
        // 使用克莱姆法则或逆矩阵求解 P1
        bezier.p1.x = det_C_inv * (X[0].x * C[1][1] - X[1].x * C[0][1]);
        bezier.p1.y = det_C_inv * (X[0].y * C[1][1] - X[1].y * C[0][1]);
        
        // 求解 P2
        bezier.p2.x = det_C_inv * (X[1].x * C[0][0] - X[0].x * C[1][0]);
        bezier.p2.y = det_C_inv * (X[1].y * C[0][0] - X[0].y * C[1][0]);
        
    } else { // 行列式为0或非常小，说明所有点可能共线
        // 使用一种简单的启发式方法来处理共线情况
        bezier.p1 = (point_f){bezier.p0.x * (2.0f/3.0f) + bezier.p3.x * (1.0f/3.0f), 
                              bezier.p0.y * (2.0f/3.0f) + bezier.p3.y * (1.0f/3.0f)};
        bezier.p2 = (point_f){bezier.p0.x * (1.0f/3.0f) + bezier.p3.x * (2.0f/3.0f), 
                              bezier.p0.y * (1.0f/3.0f) + bezier.p3.y * (2.0f/3.0f)};
    }
    
    // --- 5. 释放内存并返回结果 ---
    free(points_f);
    free(t_values);
    
    return bezier;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介      调度左右两条边的贝塞尔曲线拟合
//-------------------------------------------------------------------------------------------------------------------
void fit_edges_with_bezier(TrackContext *context) {
    // 拟合左边缘
    if (context->left_edge.is_found && context->left_edge.filtered_points_count >= 4) { // 至少需要几个点
        context->left_bezier = fit_bezier_curve(context->left_edge.filtered_edge, 
                                                context->left_edge.filtered_points_count);
        context->left_bezier_found = true;
    } else {
        context->left_bezier_found = false;
    }

    // 拟合右边缘
    if (context->right_edge.is_found && context->right_edge.filtered_points_count >= 4) {
        context->right_bezier = fit_bezier_curve(context->right_edge.filtered_edge,
                                                 context->right_edge.filtered_points_count);
        context->right_bezier_found = true;
    } else {
        context->right_bezier_found = false;
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

    // --- 4. 新增：曲线拟合阶段 ---
    fit_edges_with_bezier(context);
}