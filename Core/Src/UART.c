#include <string.h>
#include "stm32f3xx_hal.h"
#include "UART.h"

extern UART_HandleTypeDef huart1;

//DEBUG
extern UART_HandleTypeDef huart2;


static uint8_t pData, rx_buffer[256], rx_index, tx_index = 0;

void UART_Init() {
    HAL_UART_Receive_IT(&huart1, &pData, 1);
}

void UART_Transmit(uint8_t* data) {
    HAL_UART_Transmit(&huart1, data, 1, HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // HAL_UART_Transmit(&huart2, &pData, 1, HAL_MAX_DELAY);
    if (huart->Instance == USART1)
    {
        // Process received data in rx_buffer
        rx_buffer[tx_index++] = pData;

        if (tx_index >= sizeof(rx_buffer) / sizeof(rx_buffer[0])) tx_index = 0; // Prevent overflow

        // Re-enable reception for the next data
        HAL_UART_Receive_IT(&huart1, &pData, 1);
    }
}

int UART_ClearRX() {
    rx_buffer[0] = 0; // Clear the rx_buffer
    rx_index = 0;
    tx_index = 0;
    return 0;
}

uint8_t UART_GetByte() {
    uint8_t resp = rx_buffer[rx_index];
    if (rx_index == tx_index) {
        rx_buffer[rx_index++] = 0;
        if (rx_index >= sizeof(rx_buffer) / sizeof(rx_buffer[0])) rx_index = 0;
    }
    return resp;
}