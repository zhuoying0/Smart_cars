#ifndef UART_USER_H
#define UART_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "dl_uart.h"
// ֡�ṹ
typedef struct {
    int16_t error_x;    // X�����з��ţ�
    int16_t error_y;    // Y�����з��ţ�
    uint16_t distance;  // ���루�޷��ţ�
} target_frame_t;
// Э��֡�ṹ����
#define FRAME_HEADER1  0xAA
#define FRAME_HEADER2  0xAA
#define FRAME_TAIL1    0xFF
#define FRAME_TAIL2    0xFF
#define FRAME_DATA_SIZE 6  // X���H X���L Y���H Y���L ����H ����L

// �ⲿ����ȫ�ֱ���
extern volatile uart_state_t rx_state;
extern volatile uint8_t rx_order[FRAME_DATA_SIZE];

// ��������
void send_char(UART_Regs *uart, uint8_t data);
void send_str(UART_Regs *uart, uint8_t *data);
void UART0_rx_dataframe(void);
void UART_0_INST_IRQHandler(void);

void process_rx_frame(void);
#endif // UART_USER_H