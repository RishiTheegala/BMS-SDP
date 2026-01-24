#include <string.h>
#include "stm32f3xx_hal.h"
#include "UART.h"

typedef struct{
    UART_HandleTypeDef *huart;
} UART_Data;

static UART_Data uartData;

static uint8_t pData, rx_buffer[256], rx_index, tx_index = 0;

void UART_Init(UART_HandleTypeDef *huart) {
    uartData.huart = huart;
    HAL_UART_Receive_IT(uartData.huart, &pData, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != uartData.huart)
    {
        return;
    }

    // Use non-blocking transmit to avoid timing issues at high baud rates
    extern UART_HandleTypeDef huart2;
    HAL_UART_Transmit_IT(&huart2, &pData, 1);

    if (huart->Instance == uartData.huart->Instance)
    {
        // Process received data in rx_buffer
        rx_buffer[tx_index++] = pData;

        if (tx_index >= sizeof(rx_buffer) / sizeof(rx_buffer[0])) tx_index = 0; // Prevent overflow

        // Re-enable reception for the next data
        HAL_UART_Receive_IT(uartData.huart, &pData, 1);
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
    if (rx_index != tx_index) {
        rx_buffer[rx_index++] = 0;
        if (rx_index >= sizeof(rx_buffer) / sizeof(rx_buffer[0])) rx_index = 0;
    }
    return resp;
}