#ifndef UART_USER_H
#define UART_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "dl_uart.h"
// 帧结构
typedef struct {
    int16_t error_x;    // X轴误差（有符号）
    int16_t error_y;    // Y轴误差（有符号）
    uint16_t distance;  // 距离（无符号）
} target_frame_t;
// 协议帧结构定义
#define FRAME_HEADER1  0xAA
#define FRAME_HEADER2  0xAA
#define FRAME_TAIL1    0xFF
#define FRAME_TAIL2    0xFF
#define FRAME_DATA_SIZE 6  // X误差H X误差L Y误差H Y误差L 距离H 距离L

// 外部声明全局变量
extern volatile uart_state_t rx_state;
extern volatile uint8_t rx_order[FRAME_DATA_SIZE];

// 函数声明
void send_char(UART_Regs *uart, uint8_t data);
void send_str(UART_Regs *uart, uint8_t *data);
void UART0_rx_dataframe(void);
void UART_0_INST_IRQHandler(void);

void process_rx_frame(void);
#endif // UART_USER_H