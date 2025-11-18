#include "stm32f4xx_hal.h" // Or appropriate HAL library for your device
#include "UART.h"

extern UART_HandleTypeDef huart1;

static uint8_t pData, rx_buffer[256], rx_index = 0;

void send_Wake() {
    MX_GPIO_Init();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    // Reconfigure the RX pin as a GPIO Output
    GPIO_InitStruct.Pin = USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Push-pull output mode
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    HAL_GPIO_Init(USART_RX_GPIO_Port, &GPIO_InitStruct);

    // Send pulse
    HAL_GPIO_WritePin(USART_RX_GPIO_Port, USART_RX_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(USART_RX_GPIO_Port, USART_RX_Pin, GPIO_PIN_SET);

    MX_GPIO_Init();
}

void UART_Init() {
    HAL_Init();
    send_Wake();
    MX_USART1_UART_Init();
    SystemClock_Config();
    HAL_UART_Receive_IT(&huart1, pData, 1);
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
        rx_buffer[rx_index++] = pData;

        // Re-enable reception for the next data
        HAL_UART_Receive_IT(&huart1, &pData, 1);
    }
}

int UART_ClearRX() {
    for (int i = 0; i < rx_index; i++) {
        rx_buffer[i] = 0; // Clear the rx_buffer
    }
    rx_index = 0;
    return 0;
}

uint8_t* UART_GetPacket() {
    int size = rx_buffer[0];
    if (size) {
        memcpy(buffer, rx_buffer, size + 6);
        if (size + 6 == rx_index) {
            for (int i = 0; i < rx_index; i++) {
                rx_buffer[i] = 0; // Clear the rx_buffer
            }
        }
        for (int i = size + 6; i < rx_index; i++) {

            rx_buffer[i] = 0; // Clear the rx_buffer
        }
        rx_index -= size + 6; // Reset index after copying data
    }
    return buffer;
}