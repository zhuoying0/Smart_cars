#include "uart_user.h"

// ����״̬��״̬����
typedef enum {
    STATE_WAIT_HEADER1,    // �ȴ�֡ͷ1 (0xAA)
    STATE_WAIT_HEADER2,    // �ȴ�֡ͷ2 (0xAA)
    STATE_WAIT_DATA,       // �������ݲ���
    STATE_WAIT_CHECKSUM,   // �ȴ�У���
    STATE_WAIT_TAIL1,      // �ȴ�֡β1 (0xFF)
    STATE_WAIT_TAIL2       // �ȴ�֡β2 (0xFF)
} uart_state_t;
volatile uart_state_t rx_state = STATE_WAIT_HEADER1;// ��ǰ����״̬
//                            X���H X���L Y���H Y���L ����H ����L
volatile uint8_t rx_order[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};// �������ݻ�����

volatile uint8_t data_index = 0; // ��ǰ�����ֶ�����
// ���͵����ַ�
void send_char(UART_Regs *uart, uint8_t data) {
    while (DL_UART_isBusy(uart) == true); // �ȴ����ͻ���������
    DL_UART_transmitData(uart, data);
}

// �����ַ���
void send_str(UART_Regs *uart, uint8_t *data) {
    while (*data != 0) { // ����ֱ��������
        send_char(uart, *data++);
    }
}
// ȫ�ֱ�������
target_frame_t frame;

// ������յ�����������֡
void process_rx_frame(void) {
    frame.error_x = (rx_order[0] << 8) | rx_order[1]; // X���H��L
    frame.error_y = (rx_order[2] << 8) | rx_order[3]; // Y���H��L
    frame.distance = (rx_order[4] << 8) | rx_order[5]; // ����H��L
}

// UART0�����жϴ���
void UART0_rx_dataframe(void) {
    uint8_t rx_data = DL_UART_receiveData(UART_0_INST); // ��������
    static uint8_t data_index = 0; // ��������
    static checksum = 0; // У��ͳ�ʼ��
    switch (rx_state) {// ���ݵ�ǰ״̬������յ�������
        case STATE_WAIT_HEADER1:// �ȴ�֡ͷ1
            if (rx_data == FRAME_HEADER1) {
                rx_state = STATE_WAIT_HEADER2;
            }
            break;
            
        case STATE_WAIT_HEADER2:// �ȴ�֡ͷ2
            if (rx_data == FRAME_HEADER2) {
                data_index = 0; // ������������
                checksum = 0; // ����У���
                rx_state = STATE_WAIT_DATA; // �������ݽ���״̬
            } else {
                rx_state = STATE_WAIT_HEADER1; // ֡ͷ��������
            }
            break;
            
        case STATE_WAIT_DATA:// �ȴ�����
            rx_order[data_index] = rx_data; // �洢�豸ID
            checksum += rx_data; // �ۼ�У���
            data_index++;
            if(data_index >= FRAME_DATA_SIZE)
            {
                rx_state = STATE_WAIT_CHECKSUM; // ���ݽ�����ɣ�����У���״̬
            }
            break;
        case STATE_WAIT_CHECKSUM:// У��ʹ���
            if (rx_data == checksum) {
                rx_state = STATE_WAIT_TAIL1; // У�����ȷ���ȴ�֡β
            } else {
                rx_state = STATE_WAIT_HEADER1; // У��ʹ�������״̬
            }
            break;
        case STATE_WAIT_TAIL1:// �ȴ�֡β1
            if (rx_data == FRAME_TAIL1) {
                rx_state = STATE_WAIT_TAIL2;
            } else {
                rx_state = STATE_WAIT_HEADER1; // ֡β��������
            }
            break;
            
        case STATE_WAIT_TAIL2:// �ȴ�֡β2
            if (rx_data == FRAME_TAIL2) {
                process_rx_frame(); // ����֡���ճɹ�
            }
            rx_state = STATE_WAIT_HEADER1; // ���۳ɹ��������״̬
            break;
    }
}

// UART0�жϷ�����
void UART_0_INST_IRQHandler(void) {
    if (DL_UART_getPendingInterrupt(UART_0_INST) == DL_UART_IIDX_RX) {
        UART0_rx_dataframe();
    }
}