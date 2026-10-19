/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "cmsis_os.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "drv_system.h"
#include "system.h"
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

	if(DRV_SYSTEM_InitCoreFunc() != DRV_SYSTEM_STATUS_OK)
	{
		while(1);
	}
	if(SYSTEM_Init() != SYSTEM_STATUS_OK)
	{
		while(1);
	}
	if(SYSTEM_Start() != SYSTEM_STATUS_OK)
	{
		while(1);
	}

	while(1);
}




