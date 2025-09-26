#ifndef _MENU_H_
#define _MENU_H_

#include "menu_system.h"

// 模拟的图像数据和处理结果
// 在实际项目中，这些数据会由摄像头和图像处理算法更新
extern const uint8_t g_grayscale_image[128 * 64]; // 假设屏幕是128x64

// 应用程序数据结构体
typedef struct {
    float angle;
    int left_center_x, left_center_y;
    int right_center_x, right_center_y;
    char element_state[20];
    int element_stage;
    char running_status[10];
    bool show_binary_image;
} AppData;

/**
 * @brief 获取主菜单的指针
 * @return const Menu* 指向主菜单
 */
const Menu* menu_get_main_menu(void);

/**
 * @brief 更新菜单系统需要显示的实时数据
 * @param data 包含最新数据的结构体指针
 */
void menu_update_data(const AppData* data);

#endif // _MENU_H_