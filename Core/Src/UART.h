#pragma once

#include <stdint.h>

void UART_Init();
void UART_Transmit(uint8_t* data);
uint8_t* UART_GetRxData();
