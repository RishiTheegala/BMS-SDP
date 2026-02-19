#pragma once

#include <stdint.h>
#include <stdbool.h>

void CAN_Start(void);
bool CAN_Transmit(uint32_t *id, uint8_t data[]);
bool CAN_Receive(uint32_t *id, uint8_t data[]);
