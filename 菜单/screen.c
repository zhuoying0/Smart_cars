#include "screen.h"

// 包含所有可能的屏幕驱动头文件
#include "zf_device_oled.h"
#include "zf_device_tft180.h"
#include "zf_device_ips114.h"
#include "zf_device_ips200.h"
// zf_device_screen.h 内部会帮你定义 IPS200_DRIVER_TYPE，你也可以在h文件里直接修改
// 检查是否定义了且仅定义了一个屏幕
#if !defined(USE_SCREEN_OLED) && !defined(USE_SCREEN_TFT180) && !defined(USE_SCREEN_IPS114) && !defined(USE_SCREEN_IPS200)
    #error "Please define which screen to use in your config file. e.g. #define USE_SCREEN_IPS114"
#endif

#if (defined(USE_SCREEN_OLED) + defined(USE_SCREEN_TFT180) + defined(USE_SCREEN_IPS114) + defined(USE_SCREEN_IPS200)) > 1
    #error "More than one screen type is defined. Please choose only one."
#endif

//-------------------------------------------------------------------------------------------------------------------
//  函数实现
//-------------------------------------------------------------------------------------------------------------------

void screen_init(void)
{
#if defined(USE_SCREEN_OLED)
    oled_init();
#elif defined(USE_SCREEN_TFT180)
    tft180_init();
#elif defined(USE_SCREEN_IPS114)
    ips114_init();
#elif defined(USE_SCREEN_IPS200)
    // 注意：IPS200的init函数需要一个参数，我们在这里根据配置传入
    ips200_init(IPS200_DRIVER_TYPE);
#endif
}

uint16 screen_get_width(void)
{
#if defined(USE_SCREEN_OLED)
    return OLED_X_MAX;
#elif defined(USE_SCREEN_TFT180)
    return tft180_width_max;
#elif defined(USE_SCREEN_IPS114)
    return ips114_width_max;
#elif defined(USE_SCREEN_IPS200)
    return ips200_width_max;
#endif
    return 0; // fallback
}

uint16 screen_get_height(void)
{
#if defined(USE_SCREEN_OLED)
    return OLED_Y_MAX;
#elif defined(USE_SCREEN_TFT180)
    return tft180_height_max;
#elif defined(USE_SCREEN_IPS114)
    return ips114_height_max;
#elif defined(USE_SCREEN_IPS200)
    return ips200_height_max;
#endif
    return 0; // fallback
}
void screen_clear(void)
{
#if defined(USE_SCREEN_OLED)
    oled_clear();
#elif defined(USE_SCREEN_TFT180)
    tft180_clear();
#elif defined(USE_SCREEN_IPS114)
    ips114_clear();
#elif defined(USE_SCREEN_IPS200)
    ips200_clear();
#endif
}

void screen_full(const uint16 color)
{
#if defined(USE_SCREEN_OLED)
    // 对于OLED，非0为亮，0为灭
    oled_full(color ? 0xFF : 0x00);
#elif defined(USE_SCREEN_TFT180)
    tft180_full(color);
#elif defined(USE_SCREEN_IPS114)
    ips114_full(color);
#elif defined(USE_SCREEN_IPS200)
    ips200_full(color);
#endif
}

void screen_draw_point(uint16 x, uint16 y, const uint16 color)
{
#if defined(USE_SCREEN_OLED)
    // OLED的画点函数比较特殊，它一次操作8个垂直像素
    // 这里我们不实现复杂的读-改-写，只提供一个近似功能
    // 注意：这会覆盖掉(x, y/8)位置同一列的其他点
    // 若要精确画点，需要一个屏幕缓冲区(framebuffer)
    if (color) {
        oled_draw_point(x, y / 8, 1 << (y % 8));
    } else {
        // 底层库没有提供清除单个点的方法，只能清除8个点
         oled_draw_point(x, y / 8, 0x00);
    }
#elif defined(USE_SCREEN_TFT180)
    tft180_draw_point(x, y, color);
#elif defined(USE_SCREEN_IPS114)
    ips114_draw_point(x, y, color);
#elif defined(USE_SCREEN_IPS200)
    ips200_draw_point(x, y, color);
#endif
}

void screen_draw_line(uint16 x_start, uint16 y_start, uint16 x_end, uint16 y_end, const uint16 color)
{
// OLED库没有画线函数，所以我们只为支持的屏幕实现
#if defined(USE_SCREEN_TFT180)
    tft180_draw_line(x_start, y_start, x_end, y_end, color);
#elif defined(USE_SCREEN_IPS114)
    ips114_draw_line(x_start, y_start, x_end, y_end, color);
#elif defined(USE_SCREEN_IPS200)
    ips200_draw_line(x_start, y_start, x_end, y_end, color);
#endif
}


void screen_show_string(uint16 x, uint16 y, const char *str, const uint16 font_color, const uint16 bg_color)
{
#if defined(USE_SCREEN_OLED)
    // OLED是单色，忽略颜色参数
    (void)font_color; // 避免编译器警告
    (void)bg_color;
    oled_show_string(x, y, str);
#elif defined(USE_SCREEN_TFT180)
    tft180_set_color(font_color, bg_color);
    tft180_show_string(x, y, str);
#elif defined(USE_SCREEN_IPS114)
    ips114_set_color(font_color, bg_color);
    ips114_show_string(x, y, str);
#elif defined(USE_SCREEN_IPS200)
    ips200_set_color(font_color, bg_color);
    ips200_show_string(x, y, str);
#endif
}


void screen_show_int(uint16 x, uint16 y, const int32 dat, uint8 num, const uint16 font_color, const uint16 bg_color)
{
#if defined(USE_SCREEN_OLED)
    (void)font_color; (void)bg_color;
    oled_show_int(x, y, dat, num);
#else // 彩色屏通用逻辑
    #if defined(USE_SCREEN_TFT180)
    tft180_set_color(font_color, bg_color);
    tft180_show_int(x, y, dat, num);
    #elif defined(USE_SCREEN_IPS114)
    ips114_set_color(font_color, bg_color);
    ips114_show_int(x, y, dat, num);
    #elif defined(USE_SCREEN_IPS200)
    ips200_set_color(font_color, bg_color);
    ips200_show_int(x, y, dat, num);
    #endif
#endif
}


void screen_show_uint(uint16 x, uint16 y, const uint32 dat, uint8 num, const uint16 font_color, const uint16 bg_color)
{
#if defined(USE_SCREEN_OLED)
    (void)font_color; (void)bg_color;
    oled_show_uint(x, y, dat, num);
#else // 彩色屏通用逻辑
    #if defined(USE_SCREEN_TFT180)
    tft180_set_color(font_color, bg_color);
    tft180_show_uint(x, y, dat, num);
    #elif defined(USE_SCREEN_IPS114)
    ips114_set_color(font_color, bg_color);
    ips114_show_uint(x, y, dat, num);
    #elif defined(USE_SCREEN_IPS200)
    ips200_set_color(font_color, bg_color);
    ips200_show_uint(x, y, dat, num);
    #endif
#endif
}

void screen_show_float(uint16 x, uint16 y, const double dat, uint8 num, uint8 pointnum, const uint16 font_color, const uint16 bg_color)
{
#if defined(USE_SCREEN_OLED)
    (void)font_color; (void)bg_color;
    oled_show_float(x, y, dat, num, pointnum);
#else // 彩色屏通用逻辑
    #if defined(USE_SCREEN_TFT180)
    tft180_set_color(font_color, bg_color);
    tft180_show_float(x, y, dat, num, pointnum);
    #elif defined(USE_SCREEN_IPS114)
    ips114_set_color(font_color, bg_color);
    ips114_show_float(x, y, dat, num, pointnum);
    #elif defined(USE_SCREEN_IPS200)
    ips200_set_color(font_color, bg_color);
    ips200_show_float(x, y, dat, num, pointnum);
    #endif
#endif
}

void screen_show_gray_image(uint16 x, uint16 y, const uint8 *image, uint16 width, uint16 height, uint16 dis_width, uint16 dis_height, uint8 threshold)
{
#if defined(USE_SCREEN_OLED)
    oled_show_gray_image(x, y, image, width, height, dis_width, dis_height, threshold);
#elif defined(USE_SCREEN_TFT180)
    tft180_show_gray_image(x, y, image, width, height, dis_width, dis_height, threshold);
#elif defined(USE_SCREEN_IPS114)
    ips114_show_gray_image(x, y, image, width, height, dis_width, dis_height, threshold);
#elif defined(USE_SCREEN_IPS200)
    ips200_show_gray_image(x, y, image, width, height, dis_width, dis_height, threshold);
#endif
}
