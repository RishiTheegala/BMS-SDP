#include "can.h"
#include "can_agent.h"
#include "main.h"
#include "stm32f3xx_hal_can.h"
#include "stm32f3xx_hal_def.h"
#include <stdint.h>

static CAN_TxHeaderTypeDef TxHeader;
static uint32_t TxMailbox;

static CAN_RxHeaderTypeDef RxHeader;

void CAN_Start(void){
	TxHeader.IDE = CAN_ID_STD;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.DLC = 8;
}

bool CAN_Transmit(uint32_t *id, uint8_t data[]){
	TxHeader.StdId = *id;
	if(HAL_CAN_AddTxMessage(&hcan, &TxHeader, data, &TxMailbox) != HAL_OK){
		Error_Handler();
		return false;
	}
	return true;
}

bool CAN_Receive(uint32_t *id, uint8_t data[]){
	if(HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &RxHeader, data) != HAL_OK){
		Error_Handler();
		return false;
	}
	*id = RxHeader.StdId;
	return true;
}
