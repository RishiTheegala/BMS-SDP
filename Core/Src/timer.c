#include "timer.h"

TIM_HandleTypeDef htim6;

void MX_TIM6_Init(void) {
    // Enable TIM6 clock
    __HAL_RCC_TIM6_CLK_ENABLE();
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim6.Instance = TIM6;
    // Prescaler value to achieve 1 MHz clock (1 us tick)
    // Formula: Prescaler = (APB1_Clock_Freq / 1000000) - 1
    // You must ensure APB1_Clock_Freq is correctly defined/obtained (e.g., 84 MHz for some F4 devices)
    uint32_t pclk1_freq = HAL_RCC_GetPCLK1Freq(); 
    htim6.Init.Prescaler = (pclk1_freq / 1000000) - 1; 
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = 0xFFFF; // Max period for a 16-bit timer
    htim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
        // Error Handler
        while(1);
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK) {
        // Error Handler
        while(1);
    }
    // Start the timer
    HAL_TIM_Base_Start(&htim6);
}

void delay_us(int us) {
    int64_t start = __HAL_TIM_GET_COUNTER(&htim6);
    // The loop continues until the elapsed time (difference between current and start) 
    // is greater than or equal to the desired delay in microseconds.
    while((__HAL_TIM_GET_COUNTER(&htim6) - start) < us);
}