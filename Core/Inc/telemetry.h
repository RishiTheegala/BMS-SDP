#pragma once

#include <stdint.h>
typedef enum {
	VOLTAGE_INDEX = 0,
	TEMP_INDEX,
	OVUVOW_FAULT_INDEX,
	OTUT_FAULT_INDEX
} TelemetryQueryIndex_t;

void Telemetry_SendHeartbeat(void);
void Telemetry_Error(void);
void Telemetry_Respond(uint32_t id);

