/*
 * key_config.h
 * Created on: 2025-10-21
 * 说明:
 * 1. 在 "0. 平台选择" 中定义 CURRENT_PLATFORM 为你的目标平台。
 * 2. 在 "4. 硬件抽象层" 中，找到对应平台的 #elif 块。
 * 3. 在该块中，包含正确的头文件并定义按键的 GPIO Port 和 Pin。
 * 4. Key.c 和 Key.h 文件无需任何改动。
 */

#ifndef __KEY_CONFIG_H__
#define __KEY_CONFIG_H__

//=============================================================================
// 0. 平台选择
//=============================================================================

// 定义支持的平台
#define PLATFORM_STM32_HAL          1  // STM32 HAL 库 (CubeMX)
#define PLATFORM_STM32_STDPERIPH    2  // STM32 标准外设库 (StdPeriph)
#define PLATFORM_ESP32              3  // ESP32 (ESP-IDF)
// #define PLATFORM_YOUR_OWN         4  // 你可以继续添加

/**
 * @brief  <<<<< 在此处选择你当前的目标平台 >>>>>
 */
#define CURRENT_PLATFORM    PLATFORM_ESP32


//=============================================================================
// 1. 定义按键数量
//=============================================================================
#define KEY_COUNT 4    // 按键总数量
// 按键索引
#define KEY_1 0  //按键1索引
#define KEY_2 1  //按键2索引
#define KEY_3 2  //按键3索引
#define KEY_4 3  //按键4索引


//=============================================================================
// 3. 定义时间参数 (单位: ms)
//=============================================================================
#define KEY_TICK_MS                 1    // Key_Tick() 函数的调用周期, 单位ms
#define KEY_DEBOUNCE_MS             20   // 消抖时间
#define LONG_PRESS_THRESHOLD_MS     2000 // 长按时间阈值
#define DOUBLE_CLICK_THRESHOLD_MS   200  // 双击间隔时间阈值
#define REPEAT_THRESHOLD_MS         100  // 长按重复事件的间隔时间


//=============================================================================
// 4. 硬件抽象层 (HAL - Hardware Abstraction Layer)
//=============================================================================

/* * -----------------------------------------------------------------------------
 * 平台 1: STM32 HAL 库 (CubeMX)
 * -----------------------------------------------------------------------------
 * 假设: 
 * 1. 你已在 CubeMX 中将按键引脚配置为 Input 模式 (例如上拉/下拉)。
 * 2. `MX_GPIO_Init()` 会在 `Key_Init()` 之前被调用。
 * 3. 包含了正确的 HAL 库头文件 (例如 "stm32f1xx_hal.h")。
 */
#if (CURRENT_PLATFORM == PLATFORM_STM32_HAL)
    
    #include "stm32f1xx_hal.h" // 示例: F1系列, 请修改为你自己的
    
    // 2. 定义按键状态 (HAL 库)
    // 假设: 按下为低电平 (内部或外部上拉)
    #define KEY_PRESSED_STATE   GPIO_PIN_RESET  // 按键按下的电平
    #define KEY_RELEASED_STATE  GPIO_PIN_SET    // 按键松开的电平

    // -- 在此定义你的按键 GPIO --
    #define KEY1_GPIO_PORT  GPIOB
    #define KEY1_GPIO_PIN   GPIO_PIN_1
    
    #define KEY2_GPIO_PORT  GPIOB
    #define KEY2_GPIO_PIN   GPIO_PIN_2
    
    #define KEY3_GPIO_PORT  GPIOC
    #define KEY3_GPIO_PIN   GPIO_PIN_13
    
    #define KEY4_GPIO_PORT  GPIOA
    #define KEY4_GPIO_PIN   GPIO_PIN_0

    
    /**
     * @brief 硬件初始化 (HAL)
     * @note  在 HAL 库中，GPIO 初始化通常由 CubeMX 生成的 MX_GPIO_Init() 
     * 在 main.c 中统一完成，所以这里可以为空。
     */
    #define KEY_HW_Init()   // 留空，由外部 MX_GPIO_Init() 初始化

    /**
     * @brief 读取硬件状态 (HAL)
     * @note  我们使用一个 static inline 函数来提高可读性，
     * 然后用宏将其连接到 Key.c。
     */
    static inline uint8_t App_Key_Get_State(uint8_t key_index)
    {
        switch(key_index)
        {
            case KEY_1: return HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN);
            case KEY_2: return HAL_GPIO_ReadPin(KEY2_GPIO_PORT, KEY2_GPIO_PIN);
            case KEY_3: return HAL_GPIO_ReadPin(KEY3_GPIO_PORT, KEY3_GPIO_PIN);
            case KEY_4: return HAL_GPIO_ReadPin(KEY4_GPIO_PORT, KEY4_GPIO_PIN);
            default: return KEY_RELEASED_STATE; // 索引错误时返回松开状态
        }
    }
    
    #define KEY_HW_GET_STATE(key_index)   App_Key_Get_State(key_index)


/* * -----------------------------------------------------------------------------
 * 平台 2: STM32 标准外设库 (StdPeriph)
 * -----------------------------------------------------------------------------
 * 假设:
 * 1. 你使用了例如 "stm32f10x.h" 的头文件。
 * 2. 你希望本驱动自动完成 GPIO 初始化。
 */
#elif (CURRENT_PLATFORM == PLATFORM_STM32_STDPERIPH)

    #include "stm32f10x.h" // 示例: F1系列
    
    // 2. 定义按键状态 (标准库)
    // 假设: 按下为低电平 (内部或外部上拉)
    #define KEY_PRESSED_STATE   Bit_RESET  // 按键按下的电平
    #define KEY_RELEASED_STATE  Bit_SET    // 按键松开的电平

    // -- 在此定义你的按键 GPIO --
    // 假设所有按键都在 GPIOB
    #define KEY_GPIO_PORT       GPIOB
    #define KEY_GPIO_CLK        RCC_APB2Periph_GPIOB
    #define KEY1_GPIO_PIN       GPIO_Pin_1
    #define KEY2_GPIO_PIN       GPIO_Pin_2
    #define KEY3_GPIO_PIN       GPIO_Pin_10
    #define KEY4_GPIO_PIN       GPIO_Pin_11

    /**
     * @brief 硬件初始化 (标准库)
     * @note  自动初始化所有按键引脚为上拉输入
     */
    static inline void App_Key_HW_Init(void)
    {
        RCC_APB2PeriphClockCmd(KEY_GPIO_CLK, ENABLE);

        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 设置为上拉输入
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Pin = KEY1_GPIO_PIN | KEY2_GPIO_PIN | KEY3_GPIO_PIN | KEY4_GPIO_PIN;
        GPIO_Init(KEY_GPIO_PORT, &GPIO_InitStructure);
    }
    
    #define KEY_HW_Init()   App_Key_HW_Init()
    
    /**
     * @brief 读取硬件状态 (标准库)
     */
    static inline uint8_t App_Key_Get_State(uint8_t key_index)
    {
        // 假设所有按键都在同一个 PORT
        switch(key_index)
        {
            case KEY_1: return GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY1_GPIO_PIN);
            case KEY_2: return GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY2_GPIO_PIN);
            case KEY_3: return GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY3_GPIO_PIN);
            case KEY_4: return GPIO_ReadInputDataBit(KEY_GPIO_PORT, KEY4_GPIO_PIN);
            default: return KEY_RELEASED_STATE;
        }
    }
    
    #define KEY_HW_GET_STATE(key_index)   App_Key_Get_State(key_index)


/* * -----------------------------------------------------------------------------
 * 平台 3: ESP32 (ESP-IDF)
 * -----------------------------------------------------------------------------
 * 假设:
 * 1. 你使用了 ESP-IDF 框架。
 * 2. 你希望本驱动自动完成 GPIO 初始化。
 */
#elif (CURRENT_PLATFORM == PLATFORM_ESP32)

    #include "driver/gpio.h"
    
    // 2. 定义按键状态 (ESP-IDF)
    // 假设: 按下为低电平 (内部上拉)
    #define KEY_PRESSED_STATE   0  // 按键按下的电平
    #define KEY_RELEASED_STATE  1  // 按键松开的电平

    // -- 在此定义你的按键 GPIO --
    #define KEY1_GPIO_PIN   GPIO_NUM_12
    #define KEY2_GPIO_PIN   GPIO_NUM_13
    #define KEY3_GPIO_PIN   GPIO_NUM_14
    #define KEY4_GPIO_PIN   GPIO_NUM_15

    /**
     * @brief 硬件初始化 (ESP-IDF)
     * @note  自动初始化所有按键引脚为上拉输入
     */
    static inline void App_Key_HW_Init(void)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << KEY1_GPIO_PIN) | 
                            (1ULL << KEY2_GPIO_PIN) | 
                            (1ULL << KEY3_GPIO_PIN) | 
                            (1ULL << KEY4_GPIO_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,        // 启用内部上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,    // 禁用内部下拉
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
    }
    
    #define KEY_HW_Init()   App_Key_HW_Init()
    
    /**
     * @brief 读取硬件状态 (ESP-IDF)
     */
    static inline uint8_t App_Key_Get_State(uint8_t key_index)
    {
        switch(key_index)
        {
            case KEY_1: return gpio_get_level(KEY1_GPIO_PIN);
            case KEY_2: return gpio_get_level(KEY2_GPIO_PIN);
            case KEY_3: return gpio_get_level(KEY3_GPIO_PIN);
            case KEY_4: return gpio_get_level(KEY4_GPIO_PIN);
            default: return KEY_RELEASED_STATE;
        }
    }
    
    #define KEY_HW_GET_STATE(key_index)   App_Key_Get_State(key_index)

/*
 * -----------------------------------------------------------------------------
 * 错误: 未定义的平台
 * -----------------------------------------------------------------------------
 */
#else
    #error "No valid CURRENT_PLATFORM defined in key_config.h"
#endif


#endif //__KEY_CONFIG_H__