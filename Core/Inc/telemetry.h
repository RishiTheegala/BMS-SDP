#pragma once

typedef enum {
	VOLTAGE_INDEX = 0,
	TEMP_INDEX
} TelemetryQueryIndex_t;

void Telemetry_SendHeartbeat(void);
void Telemetry_Respond(void);

