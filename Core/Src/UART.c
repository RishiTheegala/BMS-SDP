#include <stdint.h>
#include "stm32f3xx_hal.h"
#include "UART.h"
#include "usart.h"
#include "stm32f3xx_hal_uart.h"
#include "stm32f3xx_hal_uart_ex.h"

#define RX_BUFFER_SIZE 256
#define DMA_RX_BUFFER_SIZE 64

typedef struct{
    UART_HandleTypeDef *huart;
} UART_Data;

static UART_Data uartData;

static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t dma_rx_buffer[DMA_RX_BUFFER_SIZE];
static volatile uint16_t dma_last_pos = 0;
static volatile uint16_t read_index = 0;
static volatile uint16_t write_index = 0;

static void UART_PushByte(uint8_t byte)
{
    uint16_t next_write = (write_index + 1U) % RX_BUFFER_SIZE;

    if (next_write == read_index)
    {
        /* Drop oldest byte when software ring buffer is full. */
        read_index = (read_index + 1U) % RX_BUFFER_SIZE;
    }

    rx_buffer[write_index] = byte;
    write_index = next_write;
}

void UART_Init(UART_HandleTypeDef *huart) {
    uartData.huart = huart;
    read_index = 0;
    write_index = 0;
    dma_last_pos = 0;

    if ((uartData.huart != NULL) &&
        (HAL_UARTEx_ReceiveToIdle_DMA(uartData.huart, dma_rx_buffer, DMA_RX_BUFFER_SIZE) == HAL_OK) &&
        (uartData.huart->hdmarx != NULL))
    {
        __HAL_DMA_DISABLE_IT(uartData.huart->hdmarx, DMA_IT_HT);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if ((uartData.huart != NULL) && (huart->Instance == uartData.huart->Instance))
    {
        uint16_t pos = Size;
        uint16_t i;

        if (pos <= DMA_RX_BUFFER_SIZE)
        {
            if (pos >= dma_last_pos)
            {
                for (i = dma_last_pos; i < pos; i++)
                {
                    UART_PushByte(dma_rx_buffer[i]);
                }
            }
            else
            {
                for (i = dma_last_pos; i < DMA_RX_BUFFER_SIZE; i++)
                {
                    UART_PushByte(dma_rx_buffer[i]);
                }
                for (i = 0; i < pos; i++)
                {
                    UART_PushByte(dma_rx_buffer[i]);
                }
            }

            dma_last_pos = pos;
            if (dma_last_pos == DMA_RX_BUFFER_SIZE)
            {
                dma_last_pos = 0;
            }
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((uartData.huart != NULL) && (huart->Instance == uartData.huart->Instance))
    {
        dma_last_pos = 0;
        if ((HAL_UARTEx_ReceiveToIdle_DMA(uartData.huart, dma_rx_buffer, DMA_RX_BUFFER_SIZE) == HAL_OK) &&
            (uartData.huart->hdmarx != NULL))
        {
            __HAL_DMA_DISABLE_IT(uartData.huart->hdmarx, DMA_IT_HT);
        }
    }
}

int UART_ClearRX(void) {
    read_index = 0;
    write_index = 0;
    return 0;
}

uint8_t UART_GetByte(void) {
    if (read_index == write_index) {
        return 0x0;
    }

    uint8_t resp = rx_buffer[read_index];
    read_index = (read_index + 1U) % RX_BUFFER_SIZE;
    return resp;
}

int UART_GetBufferSize(void) {
    if (write_index >= read_index) return write_index - read_index;
    return RX_BUFFER_SIZE - (read_index - write_index);
}