#ifndef _DEVICE_SCREEN_H_
#define _DEVICE_SCREEN_H_
#include "zf_common_headfile.h"

#include "zf_common_typedef.h"

//-------------------------------------------------------------------------------------------------------------------
// 第一步：选择你要使用的屏幕
// 在你的主工程配置文件中（例如 main.h 或 config.h 或 screen.h），定义以下宏中的一个。
// 注意：一次只能定义一个！
//-------------------------------------------------------------------------------------------------------------------
// #define USE_SCREEN_OLED
// #define USE_SCREEN_TFT180
// #define USE_SCREEN_IPS114
// #define USE_SCREEN_IPS200

// 为IPS200屏幕选择驱动方式 (如果使用IPS200，请二选一)
#if defined(USE_SCREEN_IPS200)
    #define IPS200_DRIVER_TYPE IPS200_TYPE_SPI // 或者 IPS200_TYPE_PARALLEL8
#endif


//-------------------------------------------------------------------------------------------------------------------
// 定义通用颜色（如果底层库没有，我们在这里补充）
//-------------------------------------------------------------------------------------------------------------------
#ifndef RGB565_BLACK
#define RGB565_BLACK    0x0000
#define RGB565_WHITE    0xFFFF
#define RGB565_RED      0xF800
#define RGB565_GREEN    0x07E0
#define RGB565_BLUE     0x001F
#endif


//-------------------------------------------------------------------------------------------------------------------
// 通用屏幕函数声明
// 你的 main.c 或其他业务代码应该只调用这些函数。
//-------------------------------------------------------------------------------------------------------------------

/**
 * @brief 初始化屏幕
 * @param void
 * @return void
 */
void screen_init(void);

/**
 * @brief 获取屏幕宽度
 * @return uint16_t 屏幕宽度（像素）
 */
uint16_t screen_get_width(void);

/**
 * @brief 获取屏幕高度
 * @return uint16_t 屏幕高度（像素）
 */
uint16_t screen_get_height(void);

/**
 * @brief 使用背景色清空屏幕
 * @param void
 * @return void
 */
void screen_clear(void);

/**
 * @brief 用指定颜色填充整个屏幕
 * @param color 要填充的颜色 (RGB565格式)
 * @return void
 * @note 对于OLED屏幕，非0值为点亮，0值为熄灭。
 */
void screen_full(const uint16_t color);

/**
 * @brief 在指定坐标画一个点
 * @param x X坐标
 * @param y Y坐标
 * @param color 点的颜色 (RGB565格式)
 * @return void
 * @note 对于OLED屏幕，非0值为点亮，0值为熄灭。
 */
void screen_draw_point(uint16_t x, uint16_t y, const uint16_t color);

/**
 * @brief 绘制一条直线
 * @param x_start 起点X坐标
 * @param y_start 起点Y坐标
 * @param x_end   终点X坐标
 * @param y_end   终点Y坐标
 * @param color   线的颜色 (RGB565格式)
 * @return void
 */
void screen_draw_line(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, const uint16_t color);

/**
 * @brief 显示一个字符串
 * @param x X坐标
 * @param y Y坐标
 * @param str 要显示的字符串
 * @param font_color 字体颜色 (RGB565格式)
 * @param bg_color   背景颜色 (RGB565格式)
 * @return void
 * @note 对于OLED屏幕，颜色参数被忽略，使用默认的亮/灭。
 */
void screen_show_string(uint16_t x, uint16_t y, const char *str, const uint16_t font_color, const uint16_t bg_color);

/**
 * @brief 显示一个有符号整数
 * @param x X坐标
 * @param y Y坐标
 * @param dat 要显示的数字
 * @param num 显示的位数
 * @param font_color 字体颜色
 * @param bg_color   背景颜色
 * @return void
 */
void screen_show_int(uint16_t x, uint16_t y, const int32 dat, uint8_t num, const uint16_t font_color, const uint16_t bg_color);

/**
 * @brief 显示一个无符号整数
 * @param x X坐标
 * @param y Y坐标
 * @param dat 要显示的数字
 * @param num 显示的位数
 * @param font_color 字体颜色
 * @param bg_color   背景颜色
 * @return void
 */
void screen_show_uint(uint16_t x, uint16_t y, const uint32 dat, uint8_t num, const uint16_t font_color, const uint16_t bg_color);

/**
 * @brief 显示一个浮点数
 * @param x X坐标
 * @param y Y坐标
 * @param dat 要显示的数字
 * @param num 整数位数
 * @param pointnum 小数位数
 * @param font_color 字体颜色
 * @param bg_color   背景颜色
 * @return void
 */
void screen_show_float(uint16_t x, uint16_t y, const double dat, uint8_t num, uint8_t pointnum, const uint16_t font_color, const uint16_t bg_color);

/**
 * @brief 显示灰度图像
 * @param x X坐标
 * @param y Y坐标
 * @param image 图像数据指针
 * @param width 原始图像宽度
 * @param height 原始图像高度
 * @param dis_width 显示宽度
 * @param dis_height 显示高度
 * @param threshold 二值化阈值 (0表示显示灰度)
 * @return void
 */
void screen_show_gray_image(uint16_t x, uint16_t y, const uint8_t *image, uint16_t width, uint16_t height, uint16_t dis_width, uint16_t dis_height, uint8_t threshold);


#endif /* CODE_SERVO_H_ */
