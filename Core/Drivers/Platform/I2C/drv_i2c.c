/**
 ******************************************************************************
 * @file    drv_i2c.c
 *
 * @brief   I2C driver implementation This file contains the implementation of 
 *          the I2C driver for STM32H7 microcontrollers.
 *          It provides functionality for I2C communication in master mode.
 *
 * @author  elektronika
 * @date    Apr 10, 2025
 ******************************************************************************
 */

#include "main.h"
#include "drv_i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"
/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup I2C_DRIVER I2C Driver
 * @{
 */

 /**
 * @defgroup I2C_PRIVATE_STRUCTURES I2C driver private structures
 * @{
 */

typedef struct drv_i2c_handle_t
{
	drv_i2c_instance_t					instance;   /**< I2C instance identifier */
	drv_i2c_initialization_status_t		initState;  /**< Initialization state of the I2C instance */
	drv_i2c_config_t					config;     /**< Configuration parameters for the I2C instance */
	SemaphoreHandle_t					lock;       /**< Mutex for thread-safe access to the I2C instance */
	I2C_HandleTypeDef 					deviceHandler; /**< HAL I2C handle */
}drv_i2c_handle_t;

/**
 * @}
 */

/**
 * @defgroup I2C_PRIVATE_DATA I2C driver private data
 * @{
 */

/** @brief I2C driver handle for I2C instances */
static drv_i2c_handle_t prvDRV_I2C_INSTANCES[DRV_I2C_INSTANCES_MAX_NUMBER];

/**
 * @}
 */

/**
 * @defgroup I2C_PRIVATE_FUNCTIONS I2C driver private functions
 * @{
 */

/**
 * @brief Return a prt to the handle for a given instance, or NULL if
 * 		  the instance index is out of range or not yet init.
 */
static drv_i2c_handle_t* prvDRV_I2C_GetHandle(drv_i2c_instance_t instance)
{
	if((uint32_t)instance >= DRV_I2C_INSTANCES_MAX_NUMBER) return NULL;
	return &prvDRV_I2C_INSTANCES[instance];
}

/**
 * @brief Select TIMINGR reg value that matches the req clk freq.
 * 		  Values are calculated for PCKL1 = 80MHz.
 */
static uint32_t prvDRV_I2C_GetTiming(uint32_t clkFreq)
{
	if(clkFreq <= 100000UL)
		return DRV_I2C_TIMING_100KHZ;
	else
		return DRV_I2C_TIMING_400KHZ;
}

/**
 * @}
 */

/**
 * @brief HAL MSP Init - config GPIO pins and enable preiph clocks.
 * 		  Default pin mapping:
 * 		  I2C1 : PB6 (SCL), PB7 (SDA)
 * 		  I2C2 : PB10(SLC), PB11(SDA)
 * 		  I2C3 : PC0 (SLC), PC1 (SDA)
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
	if(hi2c->Instance==I2C1)
	{
		/* USER CODE BEGIN I2C1_MspInit 0 */

		/* USER CODE END I2C1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
		PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) return;

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I2C1 GPIO Configuration
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C1_CLK_ENABLE();
	}
	else if(hi2c->Instance==I2C2)
	{
		/* USER CODE BEGIN I2C1_MspInit 0 */

		/* USER CODE END I2C1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
		PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) return;

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I2C2 GPIO Configuration
		PB10    ------> I2C2_SCL
		PB11    ------> I2C2_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C1_CLK_ENABLE();
	}
	else if(hi2c->Instance==I2C3)
	{
		/* USER CODE BEGIN I2C1_MspInit 0 */

		/* USER CODE END I2C1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C3;
		PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) return;

		__HAL_RCC_GPIOC_CLK_ENABLE();
		/**I2C3 GPIO Configuration
		PC0     ------> I2C3_SCL
		PC1     ------> I2C3_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C3_CLK_ENABLE();
	}

}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
	if(hi2c->Instance==I2C1)
	{
		/* USER CODE BEGIN I2C1_MspDeInit 0 */

		/* USER CODE END I2C1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I2C1_CLK_DISABLE();

		/**I2C1 GPIO Configuration
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
	}
	else if(hi2c->Instance==I2C2)
	{
		/* USER CODE BEGIN I2C1_MspDeInit 0 */

		/* USER CODE END I2C1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I2C2_CLK_DISABLE();

		/**I2C2 GPIO Configuration
		PB10    ------> I2C2_SCL
		PB11    ------> I2C2_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11);
	}
	else if(hi2c->Instance==I2C3)
	{
		/* USER CODE BEGIN I2C1_MspDeInit 0 */

		/* USER CODE END I2C1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I2C3_CLK_DISABLE();

		/**I2C3 GPIO Configuration
		PC0     ------> I2C3_SCL
		PC1     ------> I2C3_SDA
		*/
		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0);

		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1);
	}
}

drv_i2c_status_t 	DRV_I2C_Init()
{
	memset(prvDRV_I2C_INSTANCES, 0, sizeof(prvDRV_I2C_INSTANCES));
	return DRV_I2C_STATUS_OK;
}
drv_i2c_status_t	DRV_I2C_Instance_Init(drv_i2c_instance_t instance, drv_i2c_config_t* config)
{

	drv_i2c_handle_t* handle = prvDRV_I2C_GetHandle(instance);
	if(handle == NULL || config == NULL) return DRV_I2C_STATUS_OK;
	if(handle->initState == DRV_I2C_INITIALIZATION_STATUS_INIT) return DRV_I2C_STATUS_ERROR;

	handle->instance = instance;
	handle->config   = *config;

	switch(instance)
	{
	case DRV_I2C_INSTANCE_1: handle->deviceHandler.Instance = I2C1; break;
	case DRV_I2C_INSTANCE_2: handle->deviceHandler.Instance = I2C2; break;
	case DRV_I2C_INSTANCE_3: handle->deviceHandler.Instance = I2C3; break;
	default: return DRV_I2C_STATUS_ERROR;
	}

	handle->deviceHandler.Init.Timing 			= prvDRV_I2C_GetTiming(config->clkFreq);
	handle->deviceHandler.Init.OwnAddress1 		= 0;
	handle->deviceHandler.Init.AddressingMode 	= I2C_ADDRESSINGMODE_7BIT;
	handle->deviceHandler.Init.DualAddressMode 	= I2C_DUALADDRESS_DISABLE;
	handle->deviceHandler.Init.OwnAddress2 		= 0;
	handle->deviceHandler.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	handle->deviceHandler.Init.GeneralCallMode 	= I2C_GENERALCALL_DISABLE;
	handle->deviceHandler.Init.NoStretchMode 	= I2C_NOSTRETCH_DISABLE;

	if (HAL_I2C_Init(&handle->deviceHandler) != HAL_OK) return DRV_I2C_STATUS_ERROR;
	if (HAL_I2CEx_ConfigAnalogFilter(&handle->deviceHandler, I2C_ANALOGFILTER_ENABLE) != HAL_OK)  return DRV_I2C_STATUS_ERROR;
	if (HAL_I2CEx_ConfigDigitalFilter(&handle->deviceHandler, 0) != HAL_OK) return DRV_I2C_STATUS_ERROR;

	handle->lock = xSemaphoreCreateMutex();
	if(handle->lock == NULL) return DRV_I2C_STATUS_ERROR;

	handle->initState = DRV_I2C_INITIALIZATION_STATUS_INIT;
	return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t	DRV_I2C_Transmit(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout)
{
	drv_i2c_handle_t* handle = prvDRV_I2C_GetHandle(instance);
	if(handle == NULL || handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT) return DRV_I2C_STATUS_ERROR;

	if(xSemaphoreTake(handle->lock, portMAX_DELAY) == pdFALSE) return DRV_I2C_STATUS_ERROR;
	HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(&handle->deviceHandler, addr, data, (uint16_t)size, timeout);
	xSemaphoreGive(handle->lock);
	return (result == HAL_OK) ? DRV_I2C_STATUS_OK : DRV_I2C_STATUS_ERROR;
}

drv_i2c_status_t	DRV_I2C_Receive(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout)
{
	drv_i2c_handle_t* handle = prvDRV_I2C_GetHandle(instance);
	if(handle == NULL || handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT) return DRV_I2C_STATUS_ERROR;

	if(xSemaphoreTake(handle->lock, portMAX_DELAY) == pdFALSE) return DRV_I2C_STATUS_ERROR;
	HAL_StatusTypeDef result = HAL_I2C_Master_Receive(&handle->deviceHandler, addr, data, (uint16_t)size, timeout);
	xSemaphoreGive(handle->lock);
	return (result == HAL_OK) ? DRV_I2C_STATUS_OK : DRV_I2C_STATUS_ERROR;
}


/**
 * @}
 */
/**
 * @}
 */
