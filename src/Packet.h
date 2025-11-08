#pragma once
#include <stdint.h>

#define MAX_DATA_LENGTH 4

void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device);