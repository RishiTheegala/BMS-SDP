#include "stm32f4xx_hal.h" // Or appropriate HAL library for your device
#include "UART.h"

extern UART_HandleTypeDef huart1;

static uint8_t pData[10];

void UART_Init() {
    MX_USART2_UART_Init();
    HAL_UART_Receive_IT(&huart1, pData, 1);
}

void UART_Transmit(uint8_t* data) {
    HAL_UART_Transmit(&huart1, data, 1, HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // Process received data in rx_buffer
        // Re-enable reception for the next data
        HAL_UART_Receive_IT(&huart1, pData, 1); 
    }
}