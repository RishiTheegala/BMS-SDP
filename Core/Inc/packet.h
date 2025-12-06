#pragma once
#include <stdint.h>
#include "stm32f3xx.h"
#include "stm32f3xx_hal_uart.h"
#include "util.h"

uint8_t rx_buffers[NUM_DEVICES][256];

void Packet_Init(UART_HandleTypeDef huart);
void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device);
void ReadRegister(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length);
void DummyReadResponse(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length);