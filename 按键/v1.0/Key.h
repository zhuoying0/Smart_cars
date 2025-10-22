#ifndef __KEY_H__
#define __KEY_H__
#define GPIO_KEY_PIN 4  //假设按键连接在GPIO4



#define KEY_1 0  //按键1索引
#define KEY_2 1  //按键2索引
#define KEY_3 2  //按键3索引
#define KEY_4 3  //按键4索引
#define KEY_COUNT 4    //按键数量
//基础事件
#define KEY_HOLD        (0x01)//按住 按键按住不放时置1，按键松开时置0
#define KEY_DOWN        (0x02)//按下 按键按下的时刻置1
#define KEY_UP          (0x04)//抬起 按键松开的时刻置1
//高级事件
#define KEY_SINGLE      (0x08)//单击 按键按下松开后，没有再次按下，超过双击时间阈值的时刻置1
#define KEY_DOUBLE      (0x10)//双击 按键按下松开后，在双击时间阈值内再次按下的时刻置1
#define KEY_LONG_PRESS  (0x20)//长按 按键按住不放，超过长按时间阈值的时刻置1
#define KEY_REPEAT      (0x40)//重复 按键长按后，每隔重复时间阈值置一次1，直到按键松开

#endif //__KEY_H__
