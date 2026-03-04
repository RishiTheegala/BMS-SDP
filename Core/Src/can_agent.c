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
	uint8_t type = id >> 16;
	
	switch (type){
		case 0: // telem packet
		{
			Telemetry_Respond(id);
			break;
		}
		case 1: // command packet
		{
			uint16_t command = id & 0xFF;
			switch (command) {
				case 1:
				{
					BQ_ModuleBalancing();
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
		rxFlag = true;
		rxId = RxHeader.StdId;
		memcpy(rxBuf, RxData, 8);
	}
}
