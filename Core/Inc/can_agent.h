#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * CAN Interface:
 * ID: 
 * 	Upper Nibble: Packet Type. 0 - Telemetry, 1 - Command
 * 	Telemetry Packet:
 * 		Middle Nibble: Telemetry Type. 0 - Voltage Reading, 1 - Temperature Reading.
 * 		Lower Nibble: Cell Number.
 * 	Command Packet:
 * 		1 - Start Balancing.
 *
 */

static volatile bool rxFlag = false;
static uint32_t rxId;
static uint8_t rxBuf[8];

void CAN_Start(void);
bool CAN_Transmit(uint32_t *id, uint8_t data[]);
void CAN_HandlePacket(uint32_t id, uint8_t data[8]);
