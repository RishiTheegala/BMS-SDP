#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint8_t buffer[256];

void UART_Init();
void UART_Transmit(uint8_t* data);
uint8_t* UART_GetPacket();
int UART_ClearRX();