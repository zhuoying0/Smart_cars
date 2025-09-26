// menu_system.c

#include "menu_system.h"
#include "screen.h"
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
// 指向当前活动页面的处理器
static bool (*active_page_handler)(MenuInput input) = NULL;

// --- 内部辅助函数 ---

// 进入子菜单
static void menu_navigate_forward(void) {
    const MenuItem* selected_item = &current_menu->items[current_selection];

    if (selected_item->action) {
        selected_item->action();
        // 简单动作执行完后，需要重绘当前菜单
        menu_draw(); 
    } else if (selected_item->submenu) {
        if (stack_pointer < MAX_MENU_DEPTH - 1) {
            navigation_stack[++stack_pointer] = current_menu;
            current_menu = selected_item->submenu;
            current_selection = 0;
            menu_draw(); // 进入子菜单后重绘
        }
    } else if (selected_item->page_handler) {
        // 如果是页面，则设置active_page_handler
        active_page_handler = selected_item->page_handler;
        // 首次进入页面，用MENU_INPUT_NONE进行初始化绘制
        active_page_handler(MENU_INPUT_NONE);
    }
}

// 返回上级菜单
static void menu_navigate_back(void) {
    if (stack_pointer >= 0) {
        // 从栈中弹出上一个菜单
        current_menu = navigation_stack[stack_pointer--];
        current_selection = 0; // 返回后默认选中第一项
        menu_draw(); // 返回后重绘
    }
}

// --- 公共函数实现 ---

void menu_init(const Menu* main_menu) {
    current_menu = main_menu;
    current_selection = 0;
    stack_pointer = -1;
    active_page_handler = NULL;
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
    // 如果当前有活动页面，则将输入全部交给页面处理器
    if (active_page_handler) {
        bool stay_on_page = active_page_handler(input);
        // 如果页面处理器返回false，则退出页面
        if (!stay_on_page) {
            active_page_handler = NULL;
            menu_draw(); // 退出页面后，重绘菜单
        }
    } 
    // 否则，执行标准菜单导航逻辑
    else {
        if (!current_menu) return;
        switch (input) {
            case MENU_INPUT_UP:
                current_selection--;
                if (current_selection < 0) current_selection = current_menu->item_count - 1;
                menu_draw();
                break;
            case MENU_INPUT_DOWN:
                current_selection++;
                if (current_selection >= current_menu->item_count) current_selection = 0;
                menu_draw();
                break;
            case MENU_INPUT_ENTER:
                menu_navigate_forward();
                break;
            case MENU_INPUT_BACK:
                menu_navigate_back();
                break;
            case MENU_INPUT_NONE: // 菜单模式下忽略
                 break;
        }
    }
}