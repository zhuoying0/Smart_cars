#include "Stepper.h"

// 28BYJ-48 四相八拍 (半步) 驱动顺序
// IN1, IN2, IN3, IN4
static const uint8_t step_sequence[8][4] = {
    {1, 0, 0, 0}, // 1
    {1, 1, 0, 0}, // 2
    {0, 1, 0, 0}, // 3
    {0, 1, 1, 0}, // 4
    {0, 0, 1, 0}, // 5
    {0, 0, 1, 1}, // 6
    {0, 0, 0, 1}, // 7
    {1, 0, 0, 1}  // 8
};
// --- 全局电机管理 ---
// 这是一个指针数组，用于在ISR中访问所有已注册的电机
static Stepper_t* g_stepper_list[MAX_STEPPERS];
static uint8_t    g_stepper_count = 0;
/**
 * @brief (内部函数) 注册一个电机，使其被ISR管理
 */
static void Stepper_Register(Stepper_t* stepper)
{
    if (g_stepper_count < MAX_STEPPERS)
    {
        g_stepper_list[g_stepper_count++] = stepper;
    }
    // 你可以在这里添加一个错误处理 (e.g., else { while(1); })
}
/**
 * @brief (内部函数) 设置指定电机的4个引脚电平
 */
static void Stepper_SetPins(Stepper_t* stepper, uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4)
{
    (in1) ? GPIO_SetBits(stepper->port, stepper->in1_pin) : GPIO_ResetBits(stepper->port, stepper->in1_pin);
    (in2) ? GPIO_SetBits(stepper->port, stepper->in2_pin) : GPIO_ResetBits(stepper->port, stepper->in2_pin);
    (in3) ? GPIO_SetBits(stepper->port, stepper->in3_pin) : GPIO_ResetBits(stepper->port, stepper->in3_pin);
    (in4) ? GPIO_SetBits(stepper->port, stepper->in4_pin) : GPIO_ResetBits(stepper->port, stepper->in4_pin);
}
/**
 * @brief (内部函数) 为指定电机执行一步
 */
static void Stepper_DoStep(Stepper_t* stepper)
{
    if (stepper->direction == 1) // 正转 (CW)
    {
        stepper->current_step = (stepper->current_step + 1) % 8;
    }
    else if (stepper->direction == -1) // 反转 (CCW)
    {
        stepper->current_step = (stepper->current_step + 7) % 8;
    }

    // 设置IO电平
    Stepper_SetPins(stepper,
                    step_sequence[stepper->current_step][0],
                    step_sequence[stepper->current_step][1],
                    step_sequence[stepper->current_step][2],
                    step_sequence[stepper->current_step][3]);
}

/* -------------------- 库函数实现 -------------------- */

/**
 * @brief 初始化步进电机IO
 */
void Stepper_Init(Stepper_t* stepper, GPIO_TypeDef* port, 
                    uint16_t in1, uint16_t in2, uint16_t in3, uint16_t in4, 
                    uint16_t steps_per_rev)
{
    // 1. 保存配置到结构体
    stepper->port = port;
    stepper->in1_pin = in1;
    stepper->in2_pin = in2;
    stepper->in3_pin = in3;
    stepper->in4_pin = in4;
    stepper->steps_per_revolution = steps_per_rev;

    // 2. 初始化状态变量
    stepper->current_step = 0;
    stepper->steps_to_move = 0;
    stepper->direction = 0;
    stepper->delay_counter = 0;
    stepper->speed_delay_ms = 0;

    // 3. 配置GPIO (!! 假设时钟已在 main.c 中使能 !!)
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = stepper->in1_pin | stepper->in2_pin | stepper->in3_pin | stepper->in4_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(stepper->port, &GPIO_InitStructure);
    
    // 4. 停止电机 (所有引脚拉低)
    Stepper_SetPins(stepper, 0, 0, 0, 0);
    
    // 5. 将此电机注册到ISR管理器
    Stepper_Register(stepper);
}

/**
 * @brief 停止电机转动 (所有相断电)
 */
void Stepper_Stop(Stepper_t* stepper)
{
    // 关中断保护，防止ISR在此时修改
    __disable_irq();
    stepper->steps_to_move = 0;
    stepper->direction = 0;
    stepper->delay_counter = 0;
    __enable_irq();
    
    Stepper_SetPins(stepper, 0, 0, 0, 0); // 释放电机
}

/**
 * @brief 命令电机旋转指定的步数
 */
void Stepper_MoveSteps(Stepper_t* stepper, int32_t steps, uint16_t speed_ms_per_step)
{
    if (steps == 0) return;
    
    if (speed_ms_per_step == 0) speed_ms_per_step = 1; 

    // 关中断，确保变量设置的“原子性”
    // 防止主循环刚设置了 steps，中断就进来执行
    __disable_irq(); 
    stepper->steps_to_move = (steps > 0) ? steps : -steps;
    stepper->direction = (steps > 0) ? 1 : -1;
    stepper->speed_delay_ms = speed_ms_per_step;
    stepper->delay_counter = speed_ms_per_step; // 立即开始计时
    __enable_irq();
}

/**
 * @brief 命令电机旋转指定的角度
 */
void Stepper_MoveAngle(Stepper_t* stepper, int16_t angle, uint16_t speed_ms_per_step)
{
    int32_t steps = ((int32_t)angle * stepper->steps_per_revolution) / 360;
    Stepper_MoveSteps(stepper, steps, speed_ms_per_step);
}

/**
 * @brief 检查电机是否仍在运行
 */
uint8_t Stepper_IsRunning(Stepper_t* stepper)
{
    // 这是一个原子读操作，通常不需要关中断
    return (stepper->steps_to_move > 0);
}

/**
 * @brief (核心函数) 1ms 定时器中断处理
 * 遍历所有已注册的电机，并独立更新它们的状态
 */
void Stepper_TIM_IRQHandler(void)
{
    uint8_t i;
    Stepper_t* stepper;
    
    // 遍历所有已注册的电机
    for (i = 0; i < g_stepper_count; i++)
    {
        stepper = g_stepper_list[i];
        
        // 检查此电机是否有任务
        if (stepper->steps_to_move == 0)
        {
            continue; // 此电机空闲, 检查下一个
        }
        
        // 此电机有任务, 1ms 倒计时
        stepper->delay_counter--;
        
        if (stepper->delay_counter == 0)
        {
            // 时间到，执行一步
            Stepper_DoStep(stepper);
            
            // 剩余步数减 1
            stepper->steps_to_move--;
            
            if (stepper->steps_to_move == 0)
            {
                // 任务完成
                Stepper_SetPins(stepper, 0, 0, 0, 0); // 停止
                stepper->direction = 0;
            }
            else
            {
                // 重置延时计数器，准备下一步
                stepper->delay_counter = stepper->speed_delay_ms;
            }
        }
    }
}