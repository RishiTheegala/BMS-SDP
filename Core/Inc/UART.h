#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "stm32f3xx_hal.h"

void UART_Init(UART_HandleTypeDef *huart);
uint8_t UART_GetByte(void);
int UART_ClearRX(void);
uint8_t UART_GetBufferSize(void);