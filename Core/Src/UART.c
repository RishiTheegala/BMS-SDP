#include <stdint.h>
#include "stm32f3xx_hal.h"
#include "UART.h"
#include "usart.h"
#include "stm32f3xx_hal_uart.h"

#define RX_BUFFER_SIZE 256

typedef struct{
    UART_HandleTypeDef *huart;
} UART_Data;

static UART_Data uartData;

static uint8_t pData;
static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t read_index = 0;
static volatile uint8_t write_index = 0;

void UART_Init(UART_HandleTypeDef *huart) {
    uartData.huart = huart;
    HAL_UART_Receive_IT(uartData.huart, &pData, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == uartData.huart->Instance)
    {
        // Process received data in rx_buffer
        rx_buffer[write_index] = pData;
		write_index = (write_index + 1) % RX_BUFFER_SIZE;
        HAL_UART_Receive_IT(uartData.huart, &pData, 1);
    }
}

int UART_ClearRX(void) {
    read_index = 0;
    write_index = 0;
    return 0;
}

uint8_t UART_GetByte(void) {
	while (read_index == write_index) {
		return 0x0;
	}
	
	uint8_t resp = rx_buffer[read_index];
	read_index = (read_index + 1) % RX_BUFFER_SIZE;
    return resp;
}

uint8_t UART_GetBufferSize(void) {
    if (write_index >= read_index) return write_index - read_index;
    return RX_BUFFER_SIZE - (read_index - write_index);
}