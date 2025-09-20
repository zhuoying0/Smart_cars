#include <string.h> // 用于memset
#include "stdint.h"
IFX_ALIGN(4) uint8_t mt9v03x_image_copy[MT9V03X_H][MT9V03X_W];
void copy_image(void)
{
    memcpy(mt9v03x_image_copy, mt9v03x_image, sizeof(mt9v03x_image));
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     图像黑边添加函数
// 参数说明     *image          待处理图像指针
// 返回参数     void
// 使用示例     imageAddBlackBorder(processed_image);  // 为处理后的图像添加黑边
// 备注信息
//              - 在图像四周添加1像素宽度的黑边（0值）
//              - 顶部/底部黑边：整行填充
//              - 左右侧黑边：首尾列填充
//              - 防止后续处理时边界溢出，增强算法鲁棒性
//-------------------------------------------------------------------------------------------------------------------
void image_add_black_border(uint8_t *image,uint16_t width,uint16_t height)
{
    const uint32_t bottom_offset = width * (height - 1);
    
    // 1. 顶部/底部整行处理 (使用memset优化连续内存操作)
    memset(image, 0, width);                  // 顶部整行
    memset(image + bottom_offset, 0, width);  // 底部整行

    // 2. 左右边界处理 (单循环合并操作)
    for(uint16_t row = 1; row < height - 1; row++) {
        const uint32_t offset = row * width;
        image[offset] = 0;              // 左边界
        image[offset + width - 1] = 0; // 右边界
    }
}

typedef struct {
    uint8_t x;  // X坐标
    uint8_t y;  // Y坐标
} point;
/**
 * @brief 在图像中从下向上搜索赛道左右边界的起始点
 * * @param image   待处理的只读图像数据指针 (const uint8_t *)
 * @param p_left  用于存储左边界起点坐标的指针 (point *)
 * @param p_right 用于存储右边界起点坐标的指针 (point *)
 * @return uint8_t  如果同时找到左右边界则返回1，否则返回0
 * * @note
 * - 本函数已优化，通过指针传递结果，避免了使用全局变量。
 * - 内部循环使用指针访问代替数组下标计算，以提升性能。
 * - 左边界定义为“黑到白”的跳变点，右边界定义为“白到黑”的跳变点。
 */
uint8_t get_start_point(const uint8_t *image, point *p_left, point *p_right)
{
    int y, x;
    int l_found = 0, r_found = 0;

    // 1. 从图像底部向上逐行扫描 (y > 0 是为了避免访问黑边外的区域)
    for (y = IMAGE_H - 2; y > 0; y--)
    {
        // 获取当前扫描行的起始内存地址，为指针优化做准备
        const uint8_t *row_ptr = image + y * IMAGE_W;
        
        // 重置当前行的查找标志
        l_found = 0;
        r_found = 0;

        // 2. 处理赛道线紧贴图像边缘的特殊情况
        if (row_ptr[1] == IMAGE_WHITE && row_ptr[2] == IMAGE_WHITE)
        {
            l_found = 1;
            p_left->x = 1;
            p_left->y = y;
        }
        if (row_ptr[IMAGE_W - 2] == IMAGE_WHITE && row_ptr[IMAGE_W - 3] == IMAGE_WHITE)
        {
            r_found = 1;
            p_right->x = IMAGE_W - 2;
            p_right->y = y;
        }

        // 3. 在行内从左到右进行常规搜索
        // 使用 row_ptr[x] 代替 image[y * IMAGE_W + x]，避免了重复的乘法运算
        for (x = 1; x < (IMAGE_W - 2); x++)
        {
            // 如果左边界还没找到，则寻找 黑黑->白白 跳变
            if (l_found == 0 && row_ptr[x] == IMAGE_BLACK && row_ptr[x + 1] == IMAGE_BLACK
                             && row_ptr[x + 2] == IMAGE_WHITE && row_ptr[x + 3] == IMAGE_WHITE)
            {
                l_found = 1;
                p_left->x = x;
                p_left->y = y;
            }
            
            // 如果右边界还没找到，则寻找 白白->黑黑 跳变
            if (r_found == 0 && row_ptr[x] == IMAGE_WHITE && row_ptr[x + 1] == IMAGE_WHITE
                             && row_ptr[x + 2] == IMAGE_BLACK && row_ptr[x + 3] == IMAGE_BLACK)
            {
                r_found = 1;
                p_right->x = x;
                p_right->y = y;
            }

            // 如果在本行内左右边界都已找到，则立即跳出内层循环
            if (l_found && r_found)
            {
                break;
            }
        }

        // 4. 如果在本行成功找到了左右两个边界点，则任务完成，直接返回成功
        if (l_found && r_found)
        {
            return 1; // 成功
        }
    }

    // 5. 如果遍历完所有行都没有同时找到左右边界，则返回失败
    return 0; // 失败

}

