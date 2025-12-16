#include <string.h>
#include "stm32f3xx_hal.h"
#include "UART.h"

#define UART_RX_PIN     GPIO_PIN_10
#define UART_RX_PORT    GPIOA 

extern UART_HandleTypeDef huart1;

static uint8_t pData, rx_buffer[256], rx_index, tx_index = 0;

void send_Wake() {

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = UART_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(UART_RX_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(UART_RX_PORT, UART_RX_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(UART_RX_PORT, UART_RX_PIN, GPIO_PIN_SET);

    HAL_Delay(1);

    GPIO_InitStruct.Pin = UART_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(UART_RX_PORT, &GPIO_InitStruct);
}

void UART_Transmit(uint8_t* data) {
    HAL_UART_Transmit(&huart1, data, 1, HAL_MAX_DELAY);
    HAL_Delay(1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
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