#ifndef __STEPPER_H
#define __STEPPER_H

#include "stm32f10x.h" // 包含STM32标准库头文件

/*
 * 1. 电机参数配置
 * (28BYJ-48使用八拍法，转一圈约4096步)
 */
#define STEPS_PER_REVOLUTION 4096

/*
 * 2. 最大电机数量
 * 你可以根据需要调整，这会影响ISR的效率
 */
#define MAX_STEPPERS 2
/**
 * @brief 步进电机对象 (结构体)
 * 封装了一个电机的所有状态和配置
 */
typedef struct {
    // --- GPIO 配置 ---
    GPIO_TypeDef* port;       // GPIO 端口 (如 GPIOA)
    uint16_t      in1_pin;    // IN1 引脚 (如 GPIO_Pin_0)
    uint16_t      in2_pin;
    uint16_t      in3_pin;
    uint16_t      in4_pin;
    
    // --- 状态变量 (volatile) ---
    volatile uint8_t  current_step;     // 当前在8拍序列中的位置 (0-7)
    volatile int32_t  steps_to_move;    // 剩余步数
    volatile int8_t   direction;        // 转向: 1=正, -1=反
    volatile uint16_t speed_delay_ms;   // 每一步的目标延时(ms)
    volatile uint16_t delay_counter;    // 1ms中断的倒计时器
    
    // --- 电机参数 ---
    uint16_t steps_per_revolution; // 转一圈的总步数

} Stepper_t;

/*
 * 3. 库函数声明
 * 注意：所有函数现在都接受一个 Stepper_t 指针
 */

/**
 * @brief 初始化一个步进电机对象和其GPIO
 * @note  !!! 用户必须在此函数之前，在外部 (main.c) 手动使能GPIO端口的时钟 (RCC_APB2PeriphClockCmd) !!!
 * @param stepper 指向要初始化的电机对象 (e.g., &motorA)
 * @param port GPIO 端口 (e.g., GPIOA)
 * @param in1 ... in4 四个控制引脚
 * @param steps_per_rev 转一圈的步数 (e.g., 4096)
 */
void Stepper_Init(Stepper_t* stepper, GPIO_TypeDef* port, 
                    uint16_t in1, uint16_t in2, uint16_t in3, uint16_t in4, 
                    uint16_t steps_per_rev);

/**
 * @brief 停止指定的电机
 */
void Stepper_Stop(Stepper_t* stepper);

/**
 * @brief (核心函数) 步进电机定时器中断处理程序
 * @note  !!! 此函数必须在你的1ms定时器中断 (如 TIM2_IRQHandler) 中被调用 !!!
 * @note  它会自动管理所有已初始化的电机
 */
void Stepper_TIM_IRQHandler(void);

/**
 * @brief 命令指定的电机旋转特定步数
 */
void Stepper_MoveSteps(Stepper_t* stepper, int32_t steps, uint16_t speed_ms_per_step);

/**
 * @brief 命令指定的电机旋转特定角度
 */
void Stepper_MoveAngle(Stepper_t* stepper, int16_t angle, uint16_t speed_ms_per_step);

/**
 * @brief 检查指定的电机是否仍在运行
 */
uint8_t Stepper_IsRunning(Stepper_t* stepper);


#endif // __STEPPER_H
