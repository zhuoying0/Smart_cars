#include "stdint.h"
#include "key.h"
#define KEY_PRESSED     1 //按下
#define KEY_RELEASED    0 //松开

uint8_t Key_Flag[KEY_COUNT];
typedef enum {
    KEY_STATE_IDLE,     //空闲
    KEY_STATE_PRESSED,  //按键已按下
    KEY_STATE_RELEASED, //按键已松开
    KEY_STATE_DOUBLE,   //按键已双击
    KEY_STATE_LONG_PRESS//按键已长按
} Key_State_Machine;
void Key_Init(void)
{

}

uint8_t Get_Key_State(uint8_t key_index) 
{
    //假设按键连接在GPIO4，低电平表示按下，高电平表示松开
    //这里需要根据具体硬件平台实现读取GPIO状态的代码
    //例如：return (GPIO_ReadPin(GPIO_KEY_PIN) == 0) ? KEY_PRESSED : KEY_RELEASED;
    if(key_index == 0) {
        //模拟按键状态读取
        if(gpio_get_level(GPIO_KEY_PIN) == 0) {
            return KEY_PRESSED;
        } else {
            return KEY_RELEASED;
        }
    }
}
uint8_t Key_Check(uint8_t key_index,uint8_t event)
{
    if(Key_Flag[key_index] & event) {
        if(event != KEY_HOLD)
            Key_Flag[key_index] &= ~event; //清除事件标志
        return 1;
    }
    return 0;
}
//放入1ms定时器中调用
void Key_Tick(void) 
{
    static uint8_t Count;
    static uint8_t Curr_State[KEY_COUNT],Prev_State[KEY_COUNT];
    static Key_State_Machine Key_State[KEY_COUNT];
    
    #define LONG_PRESS_THRESHOLD 2000//按键长按时间阈值，单位ms
    #define DOUBLE_CLICK_THRESHOLD 200 //双击时间阈值，单位ms
    #define REPEAT_THRESHOLD 100 //重复时间阈值，单位ms
    static uint16_t threshold_timer[KEY_COUNT];
    for(uint8_t i=0;i<KEY_COUNT;i++) {
        if(threshold_timer[i] > 0) {
            threshold_timer[i]--;
        }
    }
    Count++;
    if(Count>=20) { //20ms定时
        Count=0;
        for(uint8_t i=0;i<KEY_COUNT;i++) {
            Prev_State[i]=Curr_State[i];
            //读取按键状态，假设函数Get_Key_State()返回1表示按下，0表示松开
            Curr_State[i]=Get_Key_State(i);

            //基础事件处理（按住、按下、抬起）
            if(Curr_State[i] == KEY_PRESSED) {
                Key_Flag[i] |= KEY_HOLD; //按住事件
            } else {
                Key_Flag[i] &= ~KEY_HOLD; //清除按住事件
            }
            if(Curr_State[i] == KEY_PRESSED && Prev_State[i] == KEY_RELEASED) {
                Key_Flag[i] |= KEY_DOWN; //按下事件(下降沿)
            }
            if(Curr_State[i] == KEY_RELEASED && Prev_State[i] == KEY_PRESSED) {
                Key_Flag[i] |= KEY_UP; //抬起事件(上升沿)
            }
            //高级事件处理（单击、双击、长按、重复）
            switch(Key_State[i]) {
                case KEY_STATE_IDLE://空闲状态
                    if(Curr_State == KEY_PRESSED) {
                        Key_State[i] = KEY_STATE_PRESSED;
                        threshold_timer[i] = LONG_PRESS_THRESHOLD; //启动长按定时器
                    }
                    break;
                case KEY_STATE_PRESSED://按下状态
                    if(Curr_State == KEY_RELEASED) {
                        Key_State[i] = KEY_STATE_RELEASED;//按键松开，进入松开状态
                        threshold_timer[i] = DOUBLE_CLICK_THRESHOLD;//启动单击/双击定时器
                    } else if(threshold_timer[i] == 0) {
                        Key_State[i] = KEY_STATE_LONG_PRESS;//长按状态
                        threshold_timer[i] = REPEAT_THRESHOLD;//启动重复定时器
                        Key_Flag[i] |= KEY_LONG_PRESS; //长按事件
                    }
                    break;
                case KEY_STATE_RELEASED://松开状态
                    if(Curr_State == KEY_PRESSED) {
                        Key_State[i] = KEY_STATE_DOUBLE;
                        Key_Flag[i] |= KEY_DOUBLE; //双击事件
                    } else if(threshold_timer[i] == 0) {

                        Key_State[i] = KEY_STATE_IDLE;
                        Key_Flag[i] |= KEY_SINGLE; //单击事件
                    }
                    break;
                case KEY_STATE_DOUBLE://双击状态
                    if(Curr_State == KEY_RELEASED) {
                        Key_State[i] = KEY_STATE_IDLE;
                    }
                    break;
                case KEY_STATE_LONG_PRESS://长按状态
                    if(Curr_State == KEY_RELEASED) {
                        Key_State[i] = KEY_STATE_IDLE;
                    } else if(threshold_timer[i] == 0) {
                        Key_Flag[i] |= KEY_REPEAT; //重复事件
                        threshold_timer[i] = REPEAT_THRESHOLD; //重置重复定时器
                    }
                    break;
                default:
                    Key_State[i] = KEY_STATE_IDLE;
                    break;
            }
        }
    }
}