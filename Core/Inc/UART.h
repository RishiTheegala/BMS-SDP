#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void send_Wake();
void UART_Init();
void UART_Transmit(uint8_t* data);
uint8_t UART_GetByte();
int UART_ClearRX();