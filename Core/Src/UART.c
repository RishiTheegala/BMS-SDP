#include <string.h>
#include "stm32f4xx_hal.h" // Or appropriate HAL library for your device
#include "UART.h"

#define UART_RX_PIN     0

extern UART_HandleTypeDef huart1;

static uint8_t pData, rx_buffer[256], rx_index, tx_index = 0;

void send_Wake() {
    MX_GPIO_Init();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    // Reconfigure the RX pin as a GPIO Output
    GPIO_InitStruct.Pin = UART_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Push-pull output mode
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Send pulse
    HAL_GPIO_WritePin(GPIOA, UART_RX_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOA, UART_RX_PIN, GPIO_PIN_SET);
}

void UART_Init() {
    HAL_Init();
    send_Wake();
}

void UART_Transmit(uint8_t* data) {
    HAL_UART_Transmit(&huart1, data, 1, HAL_MAX_DELAY);
    HAL_Delay(1); // Small delay to ensure data is sent
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