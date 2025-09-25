// menu_system.h

#ifndef _MENU_SYSTEM_H_
#define _MENU_SYSTEM_H_

#include "zf_common_typedef.h"

// 前向声明，避免头文件循环引用
struct Menu;

// 菜单项结构体
typedef struct {
    const char* text;                   // 菜单项显示的文本
    void (*action)(void);               // 关联的动作函数指针
    const struct Menu* submenu;         // 指向的子菜单，与action互斥
} MenuItem;

// 菜单结构体
typedef struct Menu {
    const MenuItem* items;              // 指向菜单项数组
    const uint8_t item_count;           // 菜单项的数量
} Menu;

// 用户输入类型枚举
typedef enum {
    MENU_INPUT_UP,
    MENU_INPUT_DOWN,
    MENU_INPUT_ENTER,
    MENU_INPUT_BACK
} MenuInput;

/**
 * @brief 初始化菜单系统
 * @param main_menu 指向你的主菜单
 */
void menu_init(const Menu* main_menu);

/**
 * @brief 绘制当前菜单到屏幕
 */
void menu_draw(void);

/**
 * @brief 处理用户输入并更新菜单状态和显示
 * @param input 用户的输入动作
 */
void menu_handle_input(MenuInput input);

#endif // _MENU_SYSTEM_H_