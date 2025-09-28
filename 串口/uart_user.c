#include "uart_user.h"

// 全局变量定义
// -----------------------------------------------------------------------------
// 当前接收状态机的状态，由中断服务程序修改
volatile uart_state_t rx_state = STATE_WAIT_HEADER1;

// 接收数据缓冲区，'volatile'关键字确保编译器不会优化掉对它的访问
volatile uint8_t rx_order[FRAME_DATA_SIZE] = {0};

// 用于存储最终解析出的完整数据帧
target_frame_t frame;


// 函数定义
// -----------------------------------------------------------------------------
/**
 * @brief 通过UART发送单个字节
 * @param uart  指向UART外设寄存器的指针 (例如 UART_0_INST)
 * @param data  要发送的8位数据
 */
void send_char(UART_Regs *uart, uint8_t data) {
    // 等待，直到UART发送缓冲区为空闲状态
    while (DL_UART_isBusy(uart) == true);
    // 将数据写入发送数据寄存器
    DL_UART_transmitData(uart, data);
}

/**
 * @brief 通过UART发送一个以'\0'结尾的字符串
 * @param uart  指向UART外设寄存器的指针 (例如 UART_0_INST)
 * @param data  指向字符串的指针
 */
void send_str(UART_Regs *uart, uint8_t *data) {
    // 循环发送直到遇到字符串结束符 '\0'
    while (*data != '\0') {
        send_char(uart, *data++);
    }
}

/**
 * @brief 处理一帧完整且校验通过的数据
 * @note  此函数在接收到完整帧的帧尾后被调用。
 * 它将缓冲区中的字节数据组合成有意义的数据结构。
 */
void process_rx_frame(void) {
    // 使用具名索引代替魔法数字，提高代码可读性和可维护性
    // 将高8位和低8位字节组合成16位有符号整数
    frame.error_x = (int16_t)((rx_order[IDX_ERROR_X_H] << 8) | rx_order[IDX_ERROR_X_L]);
    frame.error_y = (int16_t)((rx_order[IDX_ERROR_Y_H] << 8) | rx_order[IDX_ERROR_Y_L]);
    
    // 将高8位和低8位字节组合成16位无符号整数
    frame.distance = (uint16_t)((rx_order[IDX_DISTANCE_H] << 8) | rx_order[IDX_DISTANCE_L]);
}

/**
 * @brief UART0字节接收状态机
 * @note  此函数应在UART接收中断中被调用。它根据预定义的协议解析传入的字节流。
 */
void UART0_rx_dataframe(void) {
    uint8_t rx_data = DL_UART_receiveData(UART_0_INST); // 从硬件接收一个字节
    
    // static变量只在第一次进入函数时初始化，之后在函数调用之间保持其值
    static uint8_t data_index = 0; // 当前接收的数据字节索引
    static uint8_t checksum = 0;   // 累加校验和

    switch (rx_state) {
        case STATE_WAIT_HEADER1: // 等待帧头1 (0xAA)
            if (rx_data == FRAME_HEADER1) {
                rx_state = STATE_WAIT_HEADER2;
            }
            break;
            
        case STATE_WAIT_HEADER2: // 等待帧头2 (0xAA)
            if (rx_data == FRAME_HEADER2) {
                data_index = 0; // 重置数据索引
                checksum = 0;   // 重置校验和
                rx_state = STATE_WAIT_DATA; // 帧头正确，进入数据接收状态
            } else {
                rx_state = STATE_WAIT_HEADER1; // 帧头错误，重置状态机
            }
            break;
            
        case STATE_WAIT_DATA: // 接收数据部分
            rx_order[data_index] = rx_data;
            // 累加校验和，利用 uint8_t 的自动溢出特性实现模256和校验
            checksum += rx_data; 
            data_index++;
            if (data_index >= FRAME_DATA_SIZE) {
                rx_state = STATE_WAIT_CHECKSUM; // 数据接收完成，进入校验和状态
            }
            break;

        case STATE_WAIT_CHECKSUM: // 等待并校验和
            if (rx_data == checksum) {
                rx_state = STATE_WAIT_TAIL1; // 校验和正确，等待帧尾1
            } else {
                rx_state = STATE_WAIT_HEADER1; // 校验和错误，重置状态机
            }
            break;

        case STATE_WAIT_TAIL1: // 等待帧尾1 (0xFF)
            if (rx_data == FRAME_TAIL1) {
                rx_state = STATE_WAIT_TAIL2;
            } else {
                rx_state = STATE_WAIT_HEADER1; // 帧尾错误，重置状态机
            }
            break;
            
        case STATE_WAIT_TAIL2: // 等待帧尾2 (0xFF)
            if (rx_data == FRAME_TAIL2) {
                process_rx_frame(); // 帧尾正确，一帧数据接收成功，进行处理
            }
            // 无论成功与否，一帧的接收过程都已结束，重置状态机以准备接收下一帧
            rx_state = STATE_WAIT_HEADER1; 
            break;

        default: // 异常状态处理
            rx_state = STATE_WAIT_HEADER1;
            break;
    }
}

/**
 * @brief UART0 中断服务函数 (ISR)
 * @note  当中断发生时，硬件会调用此函数。
 */
void UART_0_INST_IRQHandler(void) {
    // 检查是否为接收中断标志
    if (DL_UART_getPendingInterrupt(UART_0_INST) == DL_UART_IIDX_RX) {
        // 调用状态机处理接收到的字节
        UART0_rx_dataframe();
    }
}