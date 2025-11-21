#pragma once
#include <stdint.h>
#include "stm32f3xx.h"
#include "stm32f3xx_hal_uart.h"

#define MAX_DATA_LENGTH 4

void Packet_Init(UART_HandleTypeDef huart);
void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device);
uint8_t* ReadResponse();
