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

// --- 内部状态变量 ---
// volatile 关键字是必须的，因为这些变量在中断和主循环中都会被访问

// 当前在8拍序列中的位置 (0-7)
static volatile uint8_t g_current_step = 0;

// 电机需要移动的总步数
static volatile int32_t g_steps_to_move = 0;

// 转向: 1 = 正转 (CW), -1 = 反转 (CCW)
static volatile int8_t  g_direction = 0;

// 速度控制 (ms)
static volatile uint16_t g_speed_delay_ms = 0;

// 1ms定时器中断的计数器
static volatile uint16_t g_delay_counter = 0;


/**
 * @brief 底层函数：设置4个引脚的电平
 */
static void Stepper_SetPins(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4)
{
    (in1) ? GPIO_SetBits(STEPPER_PORT, STEPPER_IN1_PIN) : GPIO_ResetBits(STEPPER_PORT, STEPPER_IN1_PIN);
    (in2) ? GPIO_SetBits(STEPPER_PORT, STEPPER_IN2_PIN) : GPIO_ResetBits(STEPPER_PORT, STEPPER_IN2_PIN);
    (in3) ? GPIO_SetBits(STEPPER_PORT, STEPPER_IN3_PIN) : GPIO_ResetBits(STEPPER_PORT, STEPPER_IN3_PIN);
    (in4) ? GPIO_SetBits(STEPPER_PORT, STEPPER_IN4_PIN) : GPIO_ResetBits(STEPPER_PORT, STEPPER_IN4_PIN);
}

/**
 * @brief 执行一步
 */
static void Stepper_DoStep(void)
{
    if (g_direction == 1) // 正转 (CW)
    {
        g_current_step = (g_current_step + 1) & 7;
    }
    else if (g_direction == -1) // 反转 (CCW)
    {
        g_current_step = (g_current_step + 7) & 7; // (g_current_step - 1 + 8) & 7
    }

    // 设置IO电平
    Stepper_SetPins(step_sequence[g_current_step][0],
                    step_sequence[g_current_step][1],
                    step_sequence[g_current_step][2],
                    step_sequence[g_current_step][3]);
}

/* -------------------- 库函数实现 -------------------- */

/**
 * @brief 初始化步进电机IO
 */
void Stepper_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 使能时钟
    RCC_APB2PeriphClockCmd(STEPPER_CLK, ENABLE);

    // 2. 配置引脚
    GPIO_InitStructure.GPIO_Pin = STEPPER_IN1_PIN | STEPPER_IN2_PIN | STEPPER_IN3_PIN | STEPPER_IN4_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STEPPER_PORT, &GPIO_InitStructure);
    
    // 3. 停止电机 (所有引脚拉低)
    Stepper_Stop();
}

/**
 * @brief 停止电机转动 (所有相断电)
 */
void Stepper_Stop(void)
{
    // 清除任务
    g_steps_to_move = 0;
    g_direction = 0;
    g_delay_counter = 0;
    
    // 所有引脚拉低，释放电机
    Stepper_SetPins(0, 0, 0, 0);
}

/**
 * @brief 命令电机旋转指定的步数
 */
void Stepper_MoveSteps(int32_t steps, uint16_t speed_ms_per_step)
{
    if (steps == 0) return;
    
    // 确保速度不为0
    if (speed_ms_per_step == 0) speed_ms_per_step = 1; 

    // 设置全局变量 (原子操作或关中断保护，但在简单系统中通常还好)
    // (如果已有任务在运行，新任务会覆盖旧任务)
    g_steps_to_move = (steps > 0) ? steps : -steps;
    g_direction = (steps > 0) ? 1 : -1;
    g_speed_delay_ms = speed_ms_per_step;
    g_delay_counter = speed_ms_per_step; // 立即开始计时
}

/**
 * @brief 命令电机旋转指定的角度
 */
void Stepper_MoveAngle(int16_t angle, uint16_t speed_ms_per_step)
{
    // 将角度转换为步数
    int32_t steps = ((int32_t)angle * STEPS_PER_REVOLUTION) / 360;
    Stepper_MoveSteps(steps, speed_ms_per_step);
}

/**
 * @brief 检查电机是否仍在运行
 */
uint8_t Stepper_IsRunning(void)
{
    return (g_steps_to_move > 0);
}

/**
 * @brief (核心函数) 步进电机定时器中断处理程序
 * @note  !!! 此函数必须在你的1ms定时器中断 (如 TIM2_IRQHandler) 中被调用 !!!
 */
void Stepper_TIM_IRQHandler(void)
{
    // 检查是否有任务在身
    if (g_steps_to_move == 0)
    {
        return; // 没有任务，直接退出
    }
    
    // 1ms 计数器
    g_delay_counter--;
    
    if (g_delay_counter == 0)
    {
        // 时间到，执行一步
        Stepper_DoStep();
        
        // 剩余步数减 1
        g_steps_to_move--;
        
        if (g_steps_to_move == 0)
        {
            // 任务完成，停止电机
            Stepper_Stop();
        }
        else
        {
            // 重置延时计数器，准备下一步
            g_delay_counter = g_speed_delay_ms;
        }
    }
}

