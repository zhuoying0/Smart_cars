#include "menu.h"
#include "param.h" // 包含参数模块
#include "zf_device_screen.h"
#include "zf_driver_delay.h"
#include <stdio.h>

// ------------------- 参数修改页面内部逻辑 -------------------

// 页面内部状态
typedef enum {
    STATE_SELECTING_PARAM, // 正在选择参数
    STATE_EDITING_PARAM    // 正在编辑参数值
} ParamEditState;

// 描述一个可编辑参数的结构体
typedef struct {
    const char* name;     // 参数名称
    void* data_ptr;       // 指向参数数据的指针
    enum { TYPE_INT, TYPE_FLOAT } type; // 参数类型
    float step_float;     // 浮点型修改步长
    int   step_int;       // 整型修改步长
} ParameterDesc;

// 页面状态变量
static ParamEditState g_edit_state;
static int g_selected_param_index;
static SystemParameters g_params_edit_buffer; // 用于编辑的临时缓冲区

// 使用此数组来定义参数页面，扩展性极强！
static ParameterDesc g_param_descriptors[] = {
    {"Base Speed", &g_params_edit_buffer.base_speed, TYPE_INT,   0.0f, 5},
    {"PID Kp",     &g_params_edit_buffer.Kp,         TYPE_FLOAT, 0.1f, 0},
    {"PID Ki",     &g_params_edit_buffer.Ki,         TYPE_FLOAT, 0.01f, 0},
    {"PID Kd",     &g_params_edit_buffer.Kd,         TYPE_FLOAT, 0.05f, 0},
    {"GKD Param",  &g_params_edit_buffer.gkd,        TYPE_FLOAT, 1.0f, 0},
};
// 计算参数数量
static const int NUM_PARAMS = sizeof(g_param_descriptors) / sizeof(ParameterDesc);

/**
 * @brief 绘制参数修改页面
 */
static void draw_param_page(void) {
    screen_clear();
    screen_show_string(5, 5, "--- Modify Parameters ---", RGB565_BLUE, RGB565_WHITE);

    char buffer[50];

    for (int i = 0; i < NUM_PARAMS; i++) {
        // 根据类型格式化数值
        char value_str[20];
        if (g_param_descriptors[i].type == TYPE_INT) {
            sprintf(value_str, "%d", *(int*)g_param_descriptors[i].data_ptr);
        } else {
            sprintf(value_str, "%.2f", *(float*)g_param_descriptors[i].data_ptr);
        }

        sprintf(buffer, "%-12s : %s", g_param_descriptors[i].name, value_str);

        uint16 y_pos = 25 + i * MENU_FONT_HEIGHT;
        
        // 根据选择和编辑状态决定颜色
        bool is_selected = (i == g_selected_param_index);
        if (is_selected) {
            // 如果是当前选中的项，并且正在编辑，使用红色文字
            if (g_edit_state == STATE_EDITING_PARAM) {
                screen_show_string(5, y_pos, buffer, RGB565_RED, RGB565_WHITE);
            } else { // 否则使用标准高亮
                screen_show_string(5, y_pos, buffer, MENU_HIGHLIGHT_TEXT_COLOR, MENU_HIGHLIGHT_COLOR);
            }
        } else {
            screen_show_string(5, y_pos, buffer, MENU_FONT_COLOR, MENU_BG_COLOR);
        }
    }

    // 绘制底部提示信息
    const char* help_text = (g_edit_state == STATE_SELECTING_PARAM) ?
        "UP/DOWN:Select ENTER:Edit" : "UP/DOWN:Change ENTER:Save";
    screen_show_string(5, screen_get_height() - MENU_FONT_HEIGHT, help_text, RGB565_BLACK, RGB565_WHITE);
}

/**
 * @brief “修改参数”页面的处理器
 */
bool page_handler_parameters(MenuInput input) {
    // 首次进入页面时的初始化
    if (input == MENU_INPUT_NONE) {
        g_edit_state = STATE_SELECTING_PARAM;
        g_selected_param_index = 0;
        // 将当前的全局参数复制到编辑缓冲区
        g_params_edit_buffer = *param_get();
    }

    // 根据状态机处理输入
    if (g_edit_state == STATE_SELECTING_PARAM) {
        switch (input) {
            case MENU_INPUT_UP:
                g_selected_param_index--;
                if (g_selected_param_index < 0) g_selected_param_index = NUM_PARAMS - 1;
                break;
            case MENU_INPUT_DOWN:
                g_selected_param_index++;
                if (g_selected_param_index >= NUM_PARAMS) g_selected_param_index = 0;
                break;
            case MENU_INPUT_ENTER:
                g_edit_state = STATE_EDITING_PARAM; // 进入编辑模式
                break;
            case MENU_INPUT_BACK:
                return false; // 退出页面
            default: break;
        }
    } else { // STATE_EDITING_PARAM
        ParameterDesc* p = &g_param_descriptors[g_selected_param_index];
        switch (input) {
            case MENU_INPUT_UP: // 增加数值
                if (p->type == TYPE_INT) *(int*)p->data_ptr += p->step_int;
                else *(float*)p->data_ptr += p->step_float;
                break;
            case MENU_INPUT_DOWN: // 减少数值
                if (p->type == TYPE_INT) *(int*)p->data_ptr -= p->step_int;
                else *(float*)p->data_ptr -= p->step_float;
                break;
            case MENU_INPUT_ENTER: // 保存并退出编辑模式
                param_update(&g_params_edit_buffer); // 将修改保存到全局参数
                g_edit_state = STATE_SELECTING_PARAM;
                break;
            case MENU_INPUT_BACK: // 取消并退出编辑模式
                g_params_edit_buffer = *param_get(); // 恢复为原始值
                g_edit_state = STATE_SELECTING_PARAM;
                break;
            default: break;
        }
    }

    // 每次操作后都重绘页面
    draw_param_page();
    return true; // 保持在页面
}

// ------------------- 简单动作函数 -------------------

void action_placeholder(void) {
    screen_clear();
    screen_show_string(10, 10, "Function Not Implemented", RGB565_RED, RGB565_WHITE);
    system_delay_ms(1500);
}

// ------------------- 菜单定义 -------------------

const MenuItem main_items[] = {
    // 这里的1和2可以替换回你之前的页面
    // {"1. Screen Display", NULL, NULL, page_handler_display},
    // {"2. Start Run",      NULL, NULL, page_handler_run},
    {"3. Modify Params",  NULL, NULL, page_handler_parameters},
    {"4. Get Centerline", action_placeholder, NULL, NULL},
};
const Menu main_menu = {main_items, sizeof(main_items) / sizeof(MenuItem)};

const Menu* menu_get_main_menu(void) {
    return &main_menu;
}