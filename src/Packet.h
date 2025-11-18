#pragma once
#include <stdint.h>
#include "util.h"

uint8_t rx_buffers[NUM_DEVICES][256];

void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device);
void ReadRegister(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length);
void DummyReadResponse(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length);