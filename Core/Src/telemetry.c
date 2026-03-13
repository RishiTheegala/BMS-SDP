#include <stdint.h>
#include "bq79656.h"
#include "util.h"
#include "can_agent.h"
#include "telemetry.h"
#include "main.h"

#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_gpio.h"

#define HEARTBEAT_ID 0xEB
#define NUM_CELLS 5

// The idea behind this telemetry is that the byte 1 of the packet
// ID corresponds to the type of data being requested and byte 0
// corresponds to the index of the array. byte 2 will be dedicated
// to a customizable prefix.

typedef struct {
	uint32_t id;
	uint8_t data[8];
} CANPacket_t;

void Telemetry_SendHeartbeat(void){
	CANPacket_t heartbeat = {
		.id = HEARTBEAT_ID,
		.data = { 0xDE, 0xAD, 0xBE, 0xEF,
				  0xC0, 0x01, 0xB0, 0xBA }
	};
	if (BQ_GetBMSFault()) {
		heartbeat.data[0] = 0xFA; // Indicate fault in heartbeat
	}
	CAN_Transmit(&heartbeat.id, heartbeat.data);
}

void Telemetry_Respond(uint32_t id){
	uint8_t dataType = (id >> 8) & 0xFF;
	uint8_t index = id & 0xFF;

	uint8_t tx_buf[8] = {0};
	uint16_t response_value = 0;

	switch(dataType){
		case VOLTAGE_INDEX:
		{
			if(index >= NUM_CELLS) return;
			response_value = BQ_GetVoltage(index);
			break;
		}
		case TEMP_INDEX:
		{
			if(index >= THERMISTORS_PER_DEVICE * NUM_BQ_DEVICES) return;
			response_value = BQ_GetTemp(index); // TODO: change to gettemp
			break;
		} 
		case OVUVOW_FAULT_INDEX:
		{
			if(index >= NUM_CELLS) return;
			response_value = BQ_GetOVUVOWFault(index);
			break;
		}
		case OTUT_FAULT_INDEX:
		{
			if(index >= NUM_BQ_DEVICES) return;
			response_value = BQ_GetOTUTFault(index);
			break;
		}
	}
	
	// copy to buffer in little endian
	tx_buf[0] = response_value & 0xFF;
	tx_buf[1] = (response_value >> 8) & 0xFF;

	CAN_Transmit(&id, tx_buf);
}

void Telemetry_Error(void) {
	uint8_t data[8] = {0};
	uint32_t error_id = FAULT_TELEMETRY;
	CAN_Transmit(&error_id, data);
}