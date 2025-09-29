#include "uart_user.hh"

// 协议常量定义 (设为模块内部，不暴露到头文件)
#define FRAME_HEADER1  0xAA
#define FRAME_HEADER2  0xAA
#define FRAME_TAIL1    0xFF
#define FRAME_TAIL2    0xFF
#define FRAME_DATA_SIZE 6

// --- 模块级静态变量 ---
// 为UART0创建一个静态的解析器实例
static uart_parser_t uart0_parser;

// --- 内部函数 ---

/**
 * @brief 将缓冲区的数据解析并组装成 target_frame_t 结构
 * @param parser 指向解析器实例的指针
 */
static void process_frame(uart_parser_t *parser) {
    target_frame_t frame;
    
    // 组合数据
    frame.error_x = (int16_t)((parser->buffer[0] << 8) | parser->buffer[1]);
    frame.error_y = (int16_t)((parser->buffer[2] << 8) | parser->buffer[3]);
    frame.distance  = (uint16_t)((parser->buffer[4] << 8) | parser->buffer[5]);

    // 如果用户注册了回调函数，则调用它
    if (parser->callback) {
        parser->callback(&frame);
    }
}


// --- 公共接口函数实现 ---

void uart_parser_init(uart_parser_t *parser, frame_handler_callback_t callback) {
    parser->state = STATE_WAIT_HEADER1;
    parser->data_index = 0;
    parser->checksum = 0;
    parser->callback = callback;
}

void uart_parser_handle_byte(uart_parser_t *parser, uint8_t byte) {
    switch (parser->state) {
        case STATE_WAIT_HEADER1:
            if (byte == FRAME_HEADER1) parser->state = STATE_WAIT_HEADER2;
            break;
        
        case STATE_WAIT_HEADER2:
            if (byte == FRAME_HEADER2) {
                parser->data_index = 0;
                parser->checksum = 0;
                parser->state = STATE_WAIT_DATA;
            } else {
                parser->state = STATE_WAIT_HEADER1;
            }
            break;
            
        case STATE_WAIT_DATA:
            parser->buffer[parser->data_index] = byte;
            parser->checksum += byte;
            parser->data_index++;
            if (parser->data_index >= FRAME_DATA_SIZE) {
                parser->state = STATE_WAIT_CHECKSUM;
            }
            break;

        case STATE_WAIT_CHECKSUM:
            if (byte == parser->checksum) {
                parser->state = STATE_WAIT_TAIL1;
            } else {
                parser->state = STATE_WAIT_HEADER1;
            }
            break;

        case STATE_WAIT_TAIL1:
            if (byte == FRAME_TAIL1) {
                parser->state = STATE_WAIT_TAIL2;
            } else {
                parser->state = STATE_WAIT_HEADER1;
            }
            break;
            
        case STATE_WAIT_TAIL2:
            if (byte == FRAME_TAIL2) {
                process_frame(parser); // 处理完整数据帧
            }
            parser->state = STATE_WAIT_HEADER1; // 无论成功与否都重置
            break;
    }
}


// --- 中断服务函数 ---

// 声明一个函数，该函数将在main.c中定义
void on_frame_received(target_frame_t *frame);

void UART_0_INST_IRQHandler(void) {
    // 检查是否为UART0的接收中断
    if (DL_UART_getPendingInterrupt(UART_0_INST) == DL_UART_IIDX_RX) {
        uint8_t received_byte = DL_UART_receiveData(UART_0_INST);
        
        // 每次中断只做一件事：将接收到的字节喂给解析器
        uart_parser_handle_byte(&uart0_parser, received_byte);
    }
}

// 在系统启动时，需要初始化这个解析器
// 可以在main函数开始的地方调用一次
void uart0_parser_setup(void) {
    uart_parser_init(&uart0_parser, on_frame_received);
}