#include "menu.h"
#include "zf_device_screen.h"
#include "zf_driver_delay.h"
#include <stdio.h>

// ------------------- 数据定义 -------------------

// 全局变量，用于存储需要被页面显示的实时数据
static AppData g_app_data;

// 模拟的图像数据
const uint8_t g_grayscale_image[128 * 64] = { /* ... 你的图像数据 ... */ };


// ------------------- 页面处理器函数 -------------------

/**
 * @brief “屏幕显示”页面的处理器
 */
bool page_handler_display(MenuInput input) {
    // 1. 处理输入
    switch (input) {
        case MENU_INPUT_DOWN:
            // 切换图像显示模式
            g_app_data.show_binary_image = !g_app_data.show_binary_image;
            break;
        case MENU_INPUT_BACK:
            return false; // 返回false，通知menu_system退出此页面
        default:
            break;
    }

    // 2. 绘制页面
    screen_clear();
    
    // -- 绘制图像 --
    uint16 img_h = screen_get_height() * 2 / 3; // 图像占屏幕高度的2/3
    uint16 img_w = screen_get_width();
    
    if (g_app_data.show_binary_image) {
        screen_show_string(0, 0, "Binary Image", RGB565_RED, RGB565_WHITE);
        screen_show_gray_image(0, 16, g_grayscale_image, 128, 64, img_w, img_h, 128); // 阈值为128
    } else {
        screen_show_string(0, 0, "Grayscale Image", RGB565_BLUE, RGB565_WHITE);
        screen_show_gray_image(0, 16, g_grayscale_image, 128, 64, img_w, img_h, 0); // 阈值为0，显示灰度
    }

    // -- 绘制下面的参数表格 --
    char buffer[50];
    uint16 table_y_start = img_h + 18;
    
    snprintf(buffer, sizeof(buffer), "Angle: %.2f", g_app_data.angle);
    screen_show_string(0, table_y_start, buffer, RGB565_BLACK, RGB565_WHITE);

    snprintf(buffer, sizeof(buffer), "L-Center: %d,%d", g_app_data.left_center_x, g_app_data.left_center_y);
    screen_show_string(0, table_y_start + 16, buffer, RGB565_BLACK, RGB565_WHITE);
    
    snprintf(buffer, sizeof(buffer), "State: %s Stage: %d", g_app_data.element_state, g_app_data.element_stage);
    screen_show_string(0, table_y_start + 32, buffer, RGB565_BLACK, RGB565_WHITE);

    return true; // 返回true，保持在此页面
}

/**
 * @brief “启动运行”页面的处理器
 */
bool page_handler_run(MenuInput input) {
    if (input == MENU_INPUT_BACK) {
        return false; // 退出页面
    }

    // 绘制页面
    screen_clear();
    char buffer[50];

    snprintf(buffer, sizeof(buffer), "Status: %s", g_app_data.running_status);
    screen_show_string(10, 20, buffer, RGB565_GREEN, RGB565_WHITE);

    snprintf(buffer, sizeof(buffer), "Element: %s", g_app_data.element_state);
    screen_show_string(10, 40, buffer, RGB565_BLACK, RGB565_WHITE);

    snprintf(buffer, sizeof(buffer), "Stage: %d", g_app_data.element_stage);
    screen_show_string(10, 60, buffer, RGB565_BLACK, RGB565_WHITE);

    return true; // 保持在此页面
}


// ------------------- 简单动作函数 -------------------

void action_placeholder(void) {
    screen_clear();
    screen_show_string(10, 10, "Function Not Implemented", RGB565_RED, RGB565_WHITE);
    system_delay_ms(1500);
}


// ------------------- 菜单定义 -------------------

const MenuItem main_items[] = {
    {"1. Screen Display", NULL, NULL, page_handler_display},
    {"2. Start Run",      NULL, NULL, page_handler_run},
    {"3. Modify Params",  action_placeholder, NULL, NULL},
    {"4. Get Centerline", action_placeholder, NULL, NULL},
};
const Menu main_menu = {main_items, sizeof(main_items) / sizeof(MenuItem)};

const Menu* menu_get_main_menu(void) {
    return &main_menu;
}

void menu_update_data(const AppData* data) {
    if(data) {
        g_app_data = *data;
    }
}