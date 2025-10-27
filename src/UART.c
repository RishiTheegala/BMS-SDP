#include "stm32f4xx_hal.h" // Or appropriate HAL library for your device
#include <stdio.h> 
#include "UART.h"
#include <stdlib.h>

extern UART_HandleTypeDef huart1;

static uint8_t pData[10];

void UART_Init() {
    MX_USART2_UART_Init();
    HAL_UART_Receive_IT(&huart1, pData, 1);
}

void UART_Transmit(uint8_t data, uint64_t device, uint64_t reg) {
    HAL_UART_Transmit(&huart1, &data, sizeof(data), HAL_MAX_DELAY);
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


uint16_t calculate_crc(uint64_t data, int length) {
    uint64_t polynomial = 0b11000000000000101;  // Polynomial from document
    int bit = length * 8 + 16;

    data ^= 0xFFFF << ((length-2)*8);
    data <<= 16;

    while (bit > 16) {
        while (!(data & (1ULL << (bit - 1)))) {
            bit--;
        }
        data ^= polynomial << bit - 17;
    }

    uint16_t crc = reverse_byte_bits((data >> 8) & 0xFF) << 8 | reverse_byte_bits(data & 0xFF);

    return crc;  // Final XOR per spec
}