#ifndef UART_USER_H
#define UART_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "dl_uart.h"

// 帧头、帧尾宏定义
#define FRAME_HEADER1  0xAA
#define FRAME_HEADER2  0xAA
#define FRAME_TAIL1    0xFF
#define FRAME_TAIL2    0xFF

// 数据帧中数据部分的长度（字节数）
#define FRAME_DATA_SIZE 6  // X误差H, X误差L, Y误差H, Y误差L, 距离H, 距离L

// 接收状态机状态定义
typedef enum {
    STATE_WAIT_HEADER1,    // 等待帧头1 (0xAA)
    STATE_WAIT_HEADER2,    // 等待帧头2 (0xAA)
    STATE_WAIT_DATA,       // 接收数据部分
    STATE_WAIT_CHECKSUM,   // 等待校验和
    STATE_WAIT_TAIL1,      // 等待帧尾1 (0xFF)
    STATE_WAIT_TAIL2       // 等待帧尾2 (0xFF)
} uart_state_t;

// 目标信息帧结构
typedef struct {
    int16_t error_x;    // X轴误差（有符号）
    int16_t error_y;    // Y轴误差（有符号）
    uint16_t distance;  // 距离（无符号）
} target_frame_t;

// 定义数据在接收缓冲区 rx_order 中的索引，提高代码可读性
typedef enum {
    IDX_ERROR_X_H,
    IDX_ERROR_X_L,
    IDX_ERROR_Y_H,
    IDX_ERROR_Y_L,
    IDX_DISTANCE_H,
    IDX_DISTANCE_L,
} frame_data_index_t;

// 外部声明全局变量
extern volatile uart_state_t rx_state;
extern volatile uint8_t rx_order[FRAME_DATA_SIZE];
extern target_frame_t frame;

// 函数声明
void send_char(UART_Regs *uart, uint8_t data);
void send_str(UART_Regs *uart, uint8_t *data);
void UART0_rx_dataframe(void);
void UART_0_INST_IRQHandler(void);
void process_rx_frame(void);

#endif // UART_USER_H