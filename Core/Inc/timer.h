#pragma once

#include "main.h"
#include "stdint.h"

extern TIM_HandleTypeDef htim6;

void MX_TIM6_Init(void);
void delay_us(int us);