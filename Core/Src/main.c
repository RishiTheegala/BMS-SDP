/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "stm32f303x8.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_uart.h"
#include "usart.h"
#include "gpio.h"
#include "bq79656.h"
#include "timer.h"
#include "iwdg.h"

#include "can_agent.h"
#include "packet.h"
#include "telemetry.h"
#include "util.h"
#include "UART.h"

#include <stdint.h>

extern UART_HandleTypeDef huart1;

void SystemClock_Config(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
	HAL_Init();

	SystemClock_Config();

	MX_GPIO_Init();
	MX_CAN_Init();
	MX_USART1_UART_Init();
	MX_USART2_UART_Init();
	MX_TIM6_Init();

	// Blink the LED
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
	HAL_Delay(50);
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
	HAL_Delay(50);
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
	HAL_Delay(50);
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);

	send_Wake(2400);
	UART_Init(&huart1);
	Packet_Init(&huart1);
	BQ_Init();
	MX_IWDG_Init();
	CAN_Start();

	uint32_t lastBQRead = HAL_GetTick();
	uint32_t lastHeartbeat = HAL_GetTick();

	while (1)
	{
		if(rxFlag){
			rxFlag = false;
			CAN_HandlePacket(rxId, rxBuf);
		}
		
		if(HAL_GetTick() - lastHeartbeat > CAN_HEARTBEAT_PERIOD_MS) {
			lastHeartbeat = HAL_GetTick();
			Telemetry_SendHeartbeat();
		}

		if(HAL_GetTick() - lastBQRead > BQ_SAMPLING_PERIOD_MS){
			lastBQRead = HAL_GetTick();

      ReadRegister(BROAD_READ, 0, FAULT_SUMMARY, 1);
      // HAL_UART_Transmit(&huart2, (uint8_t*) &rx_buffers[0][0], 1, HAL_MAX_DELAY); // Debug: send number of devices with valid data over debug UART

      // BQ_Main();

      // for(int i = 0; i < CELLS_PER_DEVICE; i++)
      // {
      //   int16_t currCellVoltage = (int16_t) (BQ_GetVoltage(i) * 1000.0F);
      //   HAL_UART_Transmit(&huart2, (uint8_t*) &currCellVoltage, 2, HAL_MAX_DELAY);
      // }

      // uint16_t newLine = 0xFF;
      // HAL_UART_Transmit(&huart2, (uint8_t*) &newLine, 2, HAL_MAX_DELAY);
    }

		if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK) {
			Error_Handler(); // Pet the watchdog
		}
	}
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  /* Enable PLL to raise SYSCLK for accurate high-speed UART */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  /* On STM32F3, use RCC_PLLSOURCE_HSI; HAL maps to HSI/2 where applicable */
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;      /* HSI (8 MHz) / 2 = 4 MHz */
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;             /* 4 MHz * 16 = 64 MHz */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* 72 MHz requires 2 wait states */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  /* Use SYSCLK (64 MHz) for USART1 to ensure accurate 1 Mbps timing */
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
