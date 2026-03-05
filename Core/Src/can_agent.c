#include "can.h"
#include "can_agent.h"
#include "main.h"
#include "bq79656.h"

#include "stm32f3xx_hal_can.h"
#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_gpio.h"
#include "telemetry.h"

#include <stdint.h>
#include <string.h>

static CAN_TxHeaderTypeDef TxHeader;
static uint32_t TxMailbox;

static CAN_RxHeaderTypeDef RxHeader;
static uint8_t RxData[8];

typedef enum {
	TELEMETRY = 0,
	COMMAND = 1,
} CAN_PacketType_t;

typedef enum {
	START_MODULE_BALANCING = 0,
	STOP_MODULE_BALANCING,
	START_CELL_BALANCING,
	STOP_CELL_BALANCING,
} CAN_CommandType_t;

void CAN_Start(void){
	TxHeader.IDE = CAN_ID_STD;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.DLC = 8;
	TxHeader.TransmitGlobalTime = DISABLE;
}

bool CAN_Transmit(uint32_t *id, uint8_t data[]){
	TxHeader.StdId = *id;
	if(HAL_CAN_AddTxMessage(&hcan, &TxHeader, data, &TxMailbox) != HAL_OK){
		Error_Handler();
		return false;
	}
	return true;
}

void CAN_HandlePacket(uint32_t id, uint8_t data[8]){
	CAN_PacketType_t type = id >> 16;
	
	switch (type){
		case TELEMETRY:
		{
			Telemetry_Respond(id);
			break;
		}
		case COMMAND:
		{
			CAN_CommandType_t command = id & 0xFF;
			switch (command) {
				case START_MODULE_BALANCING:
				{
					BQ_ModuleBalancing();
					break;
				}
				case STOP_MODULE_BALANCING:
				{
					// stop module balancing
					break;
				}
				case START_CELL_BALANCING:
				{
					BQ_HandleBalancing();
					break;
				}
				case STOP_CELL_BALANCING:
				{
					BQ_StopBalancing();
					break;
				}
				default:
				{
					break;
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *CanHandle)
{
	/* Get RX message */
	if (HAL_CAN_GetRxMessage(CanHandle, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
	{
		/* Reception Error */
		Error_Handler();
	}
	else {
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
		rxFlag = true;
		rxId = RxHeader.StdId;
		memcpy(rxBuf, RxData, 8);
	}
}
