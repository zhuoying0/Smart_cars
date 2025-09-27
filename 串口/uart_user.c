#include "uart_user.h"

// 接收状态机状态定义
typedef enum {
    STATE_WAIT_HEADER1,    // 等待帧头1 (0xAA)
    STATE_WAIT_HEADER2,    // 等待帧头2 (0xAA)
    STATE_WAIT_DATA,       // 接收数据部分
    STATE_WAIT_CHECKSUM,   // 等待校验和
    STATE_WAIT_TAIL1,      // 等待帧尾1 (0xFF)
    STATE_WAIT_TAIL2       // 等待帧尾2 (0xFF)
} uart_state_t;
volatile uart_state_t rx_state = STATE_WAIT_HEADER1;// 当前接收状态
//                            X误差H X误差L Y误差H Y误差L 距离H 距离L
volatile uint8_t rx_order[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};// 接收数据缓冲区

volatile uint8_t data_index = 0; // 当前数据字段索引
// 发送单个字符
void send_char(UART_Regs *uart, uint8_t data) {
    while (DL_UART_isBusy(uart) == true); // 等待发送缓冲区空闲
    DL_UART_transmitData(uart, data);
}

// 发送字符串
void send_str(UART_Regs *uart, uint8_t *data) {
    while (*data != 0) { // 发送直到结束符
        send_char(uart, *data++);
    }
}
// 全局变量定义
target_frame_t frame;

// 处理接收到的完整数据帧
void process_rx_frame(void) {
    frame.error_x = (rx_order[0] << 8) | rx_order[1]; // X误差H和L
    frame.error_y = (rx_order[2] << 8) | rx_order[3]; // Y误差H和L
    frame.distance = (rx_order[4] << 8) | rx_order[5]; // 距离H和L
}

// UART0接收中断处理
void UART0_rx_dataframe(void) {
    uint8_t rx_data = DL_UART_receiveData(UART_0_INST); // 接收数据
    static uint8_t data_index = 0; // 数据索引
    static checksum = 0; // 校验和初始化
    switch (rx_state) {// 根据当前状态处理接收到的数据
        case STATE_WAIT_HEADER1:// 等待帧头1
            if (rx_data == FRAME_HEADER1) {
                rx_state = STATE_WAIT_HEADER2;
            }
            break;
            
        case STATE_WAIT_HEADER2:// 等待帧头2
            if (rx_data == FRAME_HEADER2) {
                data_index = 0; // 重置数据索引
                checksum = 0; // 重置校验和
                rx_state = STATE_WAIT_DATA; // 进入数据接收状态
            } else {
                rx_state = STATE_WAIT_HEADER1; // 帧头错误，重置
            }
            break;
            
        case STATE_WAIT_DATA:// 等待数据
            rx_order[data_index] = rx_data; // 存储设备ID
            checksum += rx_data; // 累加校验和
            data_index++;
            if(data_index >= FRAME_DATA_SIZE)
            {
                rx_state = STATE_WAIT_CHECKSUM; // 数据接收完成，进入校验和状态
            }
            break;
        case STATE_WAIT_CHECKSUM:// 校验和处理
            if (rx_data == checksum) {
                rx_state = STATE_WAIT_TAIL1; // 校验和正确，等待帧尾
            } else {
                rx_state = STATE_WAIT_HEADER1; // 校验和错误，重置状态
            }
            break;
        case STATE_WAIT_TAIL1:// 等待帧尾1
            if (rx_data == FRAME_TAIL1) {
                rx_state = STATE_WAIT_TAIL2;
            } else {
                rx_state = STATE_WAIT_HEADER1; // 帧尾错误，重置
            }
            break;
            
        case STATE_WAIT_TAIL2:// 等待帧尾2
            if (rx_data == FRAME_TAIL2) {
                process_rx_frame(); // 完整帧接收成功
            }
            rx_state = STATE_WAIT_HEADER1; // 无论成功与否都重置状态
            break;
    }
}

// UART0中断服务函数
void UART_0_INST_IRQHandler(void) {
    if (DL_UART_getPendingInterrupt(UART_0_INST) == DL_UART_IIDX_RX) {
        UART0_rx_dataframe();
    }
}