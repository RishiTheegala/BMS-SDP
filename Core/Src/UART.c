#include <stdint.h>
#include "stm32f3xx_hal.h"
#include "UART.h"
#include "usart.h"
#include "stm32f3xx_hal_uart.h"
#include "stm32f3xx_hal_uart_ex.h"

#define DMA_RX_BUFFER_SIZE 512

typedef struct{
    UART_HandleTypeDef *huart;
} UART_Data;

static UART_Data uartData;

/* DMA receive buffer placed in CCMRAM for deterministic, fast access by DMA */
static volatile uint8_t dma_rx_buffer[DMA_RX_BUFFER_SIZE] __attribute__((section(".ccmram"))) __attribute__((aligned(4)));

/* dma_write_pos: index (0..DMA_RX_BUFFER_SIZE-1) where DMA has written up to (exclusive)
   dma_read_pos: index where application will read next */
static volatile uint16_t dma_write_pos = 0;
static volatile uint16_t dma_read_pos = 0;

void UART_Init(UART_HandleTypeDef *huart) {
    uartData.huart = huart;
    dma_read_pos = 0;
    dma_write_pos = 0;

    if ((uartData.huart != NULL) &&
        (HAL_UARTEx_ReceiveToIdle_DMA(uartData.huart, (uint8_t *)dma_rx_buffer, DMA_RX_BUFFER_SIZE) == HAL_OK) &&
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

        /* Size is the current write position in the DMA circular buffer. Update
           dma_write_pos so application can read directly from dma_rx_buffer. */
        if (pos <= DMA_RX_BUFFER_SIZE)
        {
            dma_write_pos = pos;
            if (dma_write_pos == DMA_RX_BUFFER_SIZE)
            {
                dma_write_pos = 0;
            }
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((uartData.huart != NULL) && (huart->Instance == uartData.huart->Instance))
    {
        dma_read_pos = 0;
        dma_write_pos = 0;
        if ((HAL_UARTEx_ReceiveToIdle_DMA(uartData.huart, (uint8_t *)dma_rx_buffer, DMA_RX_BUFFER_SIZE) == HAL_OK) &&
            (uartData.huart->hdmarx != NULL))
        {
            __HAL_DMA_DISABLE_IT(uartData.huart->hdmarx, DMA_IT_HT);
        }
    }
}

int UART_ClearRX(void) {
    dma_read_pos = dma_write_pos;
    return 0;
}

uint8_t UART_GetByte(void) {
    if (dma_read_pos == dma_write_pos) {
        return 0x0;
    }

    uint8_t resp = dma_rx_buffer[dma_read_pos];
    dma_read_pos = (uint16_t)((dma_read_pos + 1U) % DMA_RX_BUFFER_SIZE);
    return resp;
}

int UART_GetBufferSize(void) {
    uint16_t w = dma_write_pos;
    uint16_t r = dma_read_pos;
    if (w >= r) return (int)(w - r);
    return (int)(DMA_RX_BUFFER_SIZE - (r - w));
}