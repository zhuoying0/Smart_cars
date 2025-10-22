#ifndef __KEY_H__
#define __KEY_H__

#include "stdint.h"
#include "key_config.h" // 包含用户特定的配置

// 基础事件 (保持不变)
#define KEY_HOLD        (0x01) // 按住
#define KEY_DOWN        (0x02) // 按下
#define KEY_UP          (0x04) // 抬起
// 高级事件 (保持不变)
#define KEY_SINGLE      (0x08) // 单击
#define KEY_DOUBLE      (0x10) // 双击
#define KEY_LONG_PRESS  (0x20) // 长按
#define KEY_REPEAT      (0x40) // 重复

/**
 * @brief 初始化按键驱动
 * @note  会清除所有内部状态
 */
void Key_Init(void);

/**
 * @brief 按键任务滴答定时器
 * @note  必须按照 key_config.h 中定义的 KEY_TICK_MS 周期性调用
 */
void Key_Tick(void);

/**
 * @brief 检查按键事件
 * @param key_index 按键索引 (KEY_1, KEY_2, ...)
 * @param event     要检查的事件 (KEY_DOWN, KEY_SINGLE, ...)
 * @return 1: 事件发生, 0: 事件未发生
 * @note  除了 KEY_HOLD，其他事件在检查后会被自动清除
 */
uint8_t Key_Check(uint8_t key_index, uint8_t event);


#endif //__KEY_H__