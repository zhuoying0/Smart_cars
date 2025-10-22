#include "key.h"
#include "string.h" // 用于 memset

// 内部状态机定义
typedef enum {
    KEY_STATE_IDLE,     // 空闲
    KEY_STATE_PRESSED,  // 按键已按下 (等待松开或长按)
    KEY_STATE_RELEASED, // 按键已松开 (等待双击或单击超时)
    KEY_STATE_DOUBLE,   // 按键已双击 (等待松开)
    KEY_STATE_LONG_PRESS// 按键已长按 (等待松开或重复)
} Key_State_Machine_e;

// 将时间(ms)转换为滴答(ticks)
#define DEBOUNCE_TICKS        (KEY_DEBOUNCE_MS / KEY_TICK_MS)
#define LONG_PRESS_TICKS      (LONG_PRESS_THRESHOLD_MS / KEY_TICK_MS)
#define DOUBLE_CLICK_TICKS    (DOUBLE_CLICK_THRESHOLD_MS / KEY_TICK_MS)
#define REPEAT_TICKS          (REPEAT_THRESHOLD_MS / KEY_TICK_MS)

// 内部静态变量，用于追踪所有按键的状态
static uint8_t g_key_flags[KEY_COUNT];           // 事件标志位
static uint16_t g_key_timers[KEY_COUNT];          // 多功能定时器 (ms)
static Key_State_Machine_e g_key_state_machine[KEY_COUNT]; // 状态机
static uint8_t g_key_current_state[KEY_COUNT];   // 当前物理状态
static uint8_t g_key_prev_state[KEY_COUNT];      // 上一周期物理状态
static uint8_t g_debounce_count;                 // 消抖计数器

/**
 * @brief 读取硬件状态的内部函数
 */
static uint8_t Get_Key_State(uint8_t key_index) 
{
    // 调用用户实现的硬件抽象函数
    return KEY_HW_GET_STATE(key_index);
}

void Key_Init(void)
{
    KEY_HW_Init(); // 初始化硬件
    // 初始化所有状态
    memset(g_key_flags, 0, sizeof(g_key_flags));
    memset(g_key_timers, 0, sizeof(g_key_timers));
    memset(g_key_state_machine, 0, sizeof(g_key_state_machine));
    memset(g_key_current_state, KEY_RELEASED_STATE, sizeof(g_key_current_state));
    memset(g_key_prev_state, KEY_RELEASED_STATE, sizeof(g_key_prev_state));
    g_debounce_count = 0;
}

uint8_t Key_Check(uint8_t key_index, uint8_t event)
{
    if (key_index >= KEY_COUNT) return 0; // 索引越界保护

    if (g_key_flags[key_index] & event) {
        if (event != KEY_HOLD) // KEY_HOLD 是状态，不是事件，不应被清除
            g_key_flags[key_index] &= ~event; // 清除已检查的事件标志
        return 1;
    }
    return 0;
}

// 放入 KEY_TICK_MS (例如1ms) 定时器中周期性调用
void Key_Tick(void) 
{
    // 1. 更新所有按键的定时器
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        if (g_key_timers[i] > 0) {
            g_key_timers[i]--;
        }
    }

    // 2. 运行消抖状态机 (每 DEBOUNCE_TICKS 运行一次)
    if (++g_debounce_count < DEBOUNCE_TICKS) {
        return; // 未达到消抖时间
    }
    g_debounce_count = 0; // 重置消抖计数器

    // 3. 遍历所有按键，更新状态机
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        // 读取当前物理状态并保存上一周期状态
        g_key_prev_state[i] = g_key_current_state[i];
        g_key_current_state[i] = Get_Key_State(i);

        // 基础事件处理（按住、按下、抬起）
        if (g_key_current_state[i] == KEY_PRESSED_STATE) {
            g_key_flags[i] |= KEY_HOLD; // 按住事件
        } else {
            g_key_flags[i] &= ~KEY_HOLD; // 清除按住事件
        }
        
        if (g_key_current_state[i] == KEY_PRESSED_STATE && g_key_prev_state[i] == KEY_RELEASED_STATE) {
            g_key_flags[i] |= KEY_DOWN; // 按下事件(下降沿)
        }
        if (g_key_current_state[i] == KEY_RELEASED_STATE && g_key_prev_state[i] == KEY_PRESSED_STATE) {
            g_key_flags[i] |= KEY_UP; // 抬起事件(上升沿)
        }

        // 高级事件状态机处理
        switch (g_key_state_machine[i]) {
            case KEY_STATE_IDLE: // 空闲状态
                if (g_key_current_state[i] == KEY_PRESSED_STATE) {
                    g_key_state_machine[i] = KEY_STATE_PRESSED;
                    g_key_timers[i] = LONG_PRESS_TICKS; // 启动长按定时器
                }
                break;
            
            case KEY_STATE_PRESSED: // 按下状态
                if (g_key_current_state[i] == KEY_RELEASED_STATE) {
                    g_key_state_machine[i] = KEY_STATE_RELEASED; // 按键松开，进入松开状态
                    g_key_timers[i] = DOUBLE_CLICK_TICKS; // 启动单击/双击定时器
                } else if (g_key_timers[i] == 0) {
                    g_key_state_machine[i] = KEY_STATE_LONG_PRESS; // 长按状态
                    g_key_timers[i] = REPEAT_TICKS; // 启动重复定时器
                    g_key_flags[i] |= KEY_LONG_PRESS; // 触发长按事件
                }
                break;

            case KEY_STATE_RELEASED: // 松开状态
                if (g_key_current_state[i] == KEY_PRESSED_STATE) {
                    g_key_state_machine[i] = KEY_STATE_DOUBLE;
                    g_key_flags[i] |= KEY_DOUBLE; // 触发双击事件
                } else if (g_key_timers[i] == 0) {
                    g_key_state_machine[i] = KEY_STATE_IDLE;
                    g_key_flags[i] |= KEY_SINGLE; // 触发单击事件
                }
                break;

            case KEY_STATE_DOUBLE: // 双击状态
                if (g_key_current_state[i] == KEY_RELEASED_STATE) {
                    g_key_state_machine[i] = KEY_STATE_IDLE; // 等待松开后返回空闲
                }
                break;

            case KEY_STATE_LONG_PRESS: // 长按状态
                if (g_key_current_state[i] == KEY_RELEASED_STATE) {
                    g_key_state_machine[i] = KEY_STATE_IDLE;
                } else if (g_key_timers[i] == 0) {
                    g_key_flags[i] |= KEY_REPEAT; // 触发重复事件
                    g_key_timers[i] = REPEAT_TICKS; // 重置重复定时器
                }
                break;

            default:
                g_key_state_machine[i] = KEY_STATE_IDLE;
                break;
        }
    }
}