#ifndef UART_USER_H
#define UART_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "dl_uart.h"

// 目标信息帧结构 (与之前相同)
typedef struct {
    int16_t error_x;
    int16_t error_y;
    uint16_t distance;
} target_frame_t;

// 定义一个函数指针类型，用于处理接收到的数据帧
// 当一帧有效数据被成功解析后，此类型的函数将被调用
typedef void (*frame_handler_callback_t)(target_frame_t *frame);

// UART协议解析器结构体
// 封装了状态机所需的所有数据
typedef struct {
    // 内部状态
    enum {
        STATE_WAIT_HEADER1,
        STATE_WAIT_HEADER2,
        STATE_WAIT_DATA,
        STATE_WAIT_CHECKSUM,
        STATE_WAIT_TAIL1,
        STATE_WAIT_TAIL2
    } state;

    // 内部数据
    uint8_t buffer[6];
    uint8_t data_index;
    uint8_t checksum;

    // 回调函数
    frame_handler_callback_t callback; // 指向用户处理函数的指针

} uart_parser_t;


// --- 公共接口函数 ---

/**
 * @brief 初始化UART协议解析器
 * @param parser   指向要初始化的解析器实例的指针
 * @param callback 当接收到完整数据帧时要调用的函数
 */
void uart_parser_init(uart_parser_t *parser, frame_handler_callback_t callback);

/**
 * @brief 向状态机送入一个字节进行处理
 * @param parser 指向解析器实例的指针
 * @param byte   从UART接收到的单个字节
 */
void uart_parser_handle_byte(uart_parser_t *parser, uint8_t byte);

// UART中断服务函数依然需要暴露给启动文件
void UART_0_INST_IRQHandler(void);

#endif // UART_USER_H