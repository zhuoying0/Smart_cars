#ifndef __STEPPER_H
#define __STEPPER_H

#include "stm32f10x.h" // 包含STM32标准库头文件

/*
 * 1. GPIO 配置
 * 在这里修改你的GPIO端口和引脚
 */
#define STEPPER_PORT     GPIOA
#define STEPPER_CLK      RCC_APB2Periph_GPIOA

#define STEPPER_IN1_PIN  GPIO_Pin_0
#define STEPPER_IN2_PIN  GPIO_Pin_1
#define STEPPER_IN3_PIN  GPIO_Pin_2
#define STEPPER_IN4_PIN  GPIO_Pin_3

/*
 * 2. 电机参数配置
 * (28BYJ-48使用八拍法，转一圈约4096步)
 */
#define STEPS_PER_REVOLUTION 4096


/*
 * 3. 库函数声明
 */

/**
 * @brief 初始化步进电机IO
 */
void Stepper_Init(void);

/**
 * @brief 停止电机转动 (所有相断电)
 * @note  也会清除所有待处理的移动任务
 */
void Stepper_Stop(void);

/**
 * @brief (核心函数) 步进电机定时器中断处理程序
 * @note  !!! 此函数必须在你的1ms定时器中断 (如 TIM2_IRQHandler) 中被调用 !!!
 */
void Stepper_TIM_IRQHandler(void);

/**
 * @brief 命令电机旋转指定的步数
 * @param steps 步数。正数: 正转 (CW), 负数: 反转 (CCW)
 * @param speed_ms_per_step 每一步之间的毫秒数 (值越小, 转速越快, 推荐 2-5)
 */
void Stepper_MoveSteps(int32_t steps, uint16_t speed_ms_per_step);

/**
 * @brief 命令电机旋转指定的角度
 * @param angle 角度。正数: 正转 (CW), 负数: 反转 (CCW)
 * @param speed_ms_per_step 每一步之间的毫秒数 (值越小, 转速越快, 推荐 2-5)
 */
void Stepper_MoveAngle(int16_t angle, uint16_t speed_ms_per_step);

/**
 * @brief 检查电机是否仍在运行
 * @retval 1 (true) 电机正在执行任务
 * @retval 0 (false) 电机已停止
 */
uint8_t Stepper_IsRunning(void);


#endif // __STEPPER_H
