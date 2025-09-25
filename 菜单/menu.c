// menu_system.c

#include "menu_system.h"
#include "zf_device_screen.h" // 仅依赖通用屏幕接口
#include <string.h>
#include <stdio.h>

// --- 菜单外观配置 ---
#define MENU_FONT_COLOR         RGB565_BLACK
#define MENU_BG_COLOR           RGB565_WHITE
#define MENU_HIGHLIGHT_COLOR    RGB565_BLUE
#define MENU_HIGHLIGHT_TEXT_COLOR RGB565_WHITE
#define MENU_FONT_HEIGHT        16 // 假设使用8x16字体
#define MENU_TOP_PADDING        5

// --- 导航栈配置 ---
#define MAX_MENU_DEPTH 5 // 菜单最大嵌套深度

// --- 内部状态变量 ---
static const Menu* current_menu;
static int current_selection;

// 导航栈
static const Menu* navigation_stack[MAX_MENU_DEPTH];
static int stack_pointer = -1; // -1表示栈空

// --- 内部辅助函数 ---

// 进入子菜单
static void menu_navigate_forward(void) {
    if (!current_menu) return;
    const MenuItem* selected_item = &current_menu->items[current_selection];

    if (selected_item->action) {
        // 如果是动作项，执行动作
        selected_item->action();
        // 动作执行后，可能需要重新绘制菜单（例如动作改变了某些状态）
        // menu_draw(); // 在handle_input中统一绘制
    } else if (selected_item->submenu) {
        // 如果是子菜单项，导航到子菜单
        if (stack_pointer < MAX_MENU_DEPTH - 1) {
            // 将当前菜单压入栈
            navigation_stack[++stack_pointer] = current_menu;
            // 更新当前菜单
            current_menu = selected_item->submenu;
            current_selection = 0;
        }
    }
}

// 返回上级菜单
static void menu_navigate_back(void) {
    if (stack_pointer >= 0) {
        // 从栈中弹出上一个菜单
        current_menu = navigation_stack[stack_pointer--];
        current_selection = 0; // 返回后默认选中第一项
    }
}

// --- 公共函数实现 ---

void menu_init(const Menu* main_menu) {
    current_menu = main_menu;
    current_selection = 0;
    stack_pointer = -1;
}

void menu_draw(void) {
    if (!current_menu) return;

    screen_clear();

    char display_buffer[40]; // 用于格式化字符串

    for (int i = 0; i < current_menu->item_count; ++i) {
        // 计算Y坐标
        uint16 y_pos = MENU_TOP_PADDING + i * MENU_FONT_HEIGHT;

        // 格式化显示的文本，前面加一个指示符'>'或' '
        snprintf(display_buffer, sizeof(display_buffer), "%c %s",
                 (i == current_selection) ? '>' : ' ',
                 current_menu->items[i].text);

        if (i == current_selection) {
            // 高亮项：使用高亮背景和文字颜色
            screen_show_string(5, y_pos, display_buffer, MENU_HIGHLIGHT_TEXT_COLOR, MENU_HIGHLIGHT_COLOR);
        } else {
            // 普通项
            screen_show_string(5, y_pos, display_buffer, MENU_FONT_COLOR, MENU_BG_COLOR);
        }
    }
}

void menu_handle_input(MenuInput input) {
    if (!current_menu) return;

    switch (input) {
        case MENU_INPUT_UP:
            current_selection--;
            if (current_selection < 0) {
                current_selection = current_menu->item_count - 1;
            }
            break;
        case MENU_INPUT_DOWN:
            current_selection++;
            if (current_selection >= current_menu->item_count) {
                current_selection = 0;
            }
            break;
        case MENU_INPUT_ENTER:
            menu_navigate_forward();
            break;
        case MENU_INPUT_BACK:
            menu_navigate_back();
            break;
    }

    // 任何输入操作后都重新绘制菜单
    menu_draw();
}