/**
 ******************************************************************************
 * @file    drv_uart.c
 *
 * @brief   UART driver implementation providing hardware abstraction layer for 
 *          STM32 UART peripherals including UART1, UART3, UART4, UART6, and UART7.
 *          This driver supports UART initialization, configuration of baud
 *          rate, parity, and stop bits, data transmission with timeout control,
 *          and interrupt-driven data reception with callback mechanisms.
 *          The driver enables reliable serial communication for various
 *          applications including debugging, data logging, and communication
 *          with external devices.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2023
 ******************************************************************************
 */
#include "main.h"

#include "FreeRTOS.h"
#include "semphr.h"


#include "drv_uart.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup UART_DRIVER UART Driver
 * @{
 */

/**
 * @defgroup UART_PRIVATE_STRUCTURES UART driver private structures
 * @{
 */
/**
 * @brief UART driver internal handle structure
 */
typedef struct
{
	drv_uart_instance_t 				instance;      /**< UART instance identifier */
	drv_uart_initialization_status_t	initState;     /**< UART initialization state */
	drv_uart_config_t					config;        /**< UART configuration parameters */
	SemaphoreHandle_t					lock;          /**< Mutex for thread-safe operations */
	UART_HandleTypeDef 					deviceHandler;  /**< HAL UART handle */
}drv_uart_handle_t;
/**
 * @}
 */

/**
 * @defgroup UART_PRIVATE_DATA UART driver private data
 * @{
 */
static drv_uart_handle_t 		prvDRV_UART_INSTANCES[CONF_UART_INSTANCES_MAX_NUMBER];  /**< UART instances array */
static drv_uart_rx_isr_callback prvDRV_UART_CALLBACKS[CONF_UART_INSTANCES_MAX_NUMBER];  /**< UART RX callbacks array */
static /*volatile*/	uint8_t 		data;                                                  /**< Temporary storage for received data */
/**
 * @}
 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

	if(huart->Instance == UART4)
	{
		prvDRV_UART_CALLBACKS[DRV_UART_INSTANCE_4](data);
		HAL_UART_Receive_IT(&prvDRV_UART_INSTANCES[DRV_UART_INSTANCE_4].deviceHandler, &data, 1);
	}

}


void UART4_IRQHandler(void)
{
  /* USER CODE BEGIN UART4_IRQn 0 */

  /* USER CODE END UART4_IRQn 0 */
  HAL_UART_IRQHandler(&prvDRV_UART_INSTANCES[DRV_UART_INSTANCE_4].deviceHandler);
  /* USER CODE BEGIN UART4_IRQn 1 */

  /* USER CODE END UART4_IRQn 1 */
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
	 GPIO_InitTypeDef GPIO_InitStruct = {0};
	  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	  if(huart->Instance==UART4)
	  {
	    /* USER CODE BEGIN UART4_MspInit 0 */

	    /* USER CODE END UART4_MspInit 0 */

	  /** Initializes the peripherals clock
	  */
	    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_UART4;
	    PeriphClkInit.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
	    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	    {
	      Error_Handler();
	    }

	    /* Peripheral clock enable */
	    __HAL_RCC_UART4_CLK_ENABLE();

	    __HAL_RCC_GPIOC_CLK_ENABLE();
	    /**UART4 GPIO Configuration
	    PC10     ------> UART4_TX
	    PC11     ------> UART4_RX
	    */
	    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
	    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	    GPIO_InitStruct.Pull = GPIO_NOPULL;
	    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
	    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	    /* USER CODE BEGIN UART4_MspInit 1 */

	    /* USER CODE END UART4_MspInit 1 */

	  }
}

void HAL_USART_MspDeInit(UART_HandleTypeDef* huart)
{
	 if(huart->Instance==UART4)
	  {
	    /* USER CODE BEGIN UART4_MspDeInit 0 */

	    /* USER CODE END UART4_MspDeInit 0 */
	    /* Peripheral clock disable */
	    __HAL_RCC_UART4_CLK_DISABLE();

	    /**UART4 GPIO Configuration
	    PC10     ------> UART4_TX
	    PC11     ------> UART4_RX
	    */
	    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10|GPIO_PIN_11);

	    /* USER CODE BEGIN UART4_MspDeInit 1 */

	    /* USER CODE END UART4_MspDeInit 1 */
	  }
}

drv_uart_status_t	DRV_UART_Init()
{
	memset(prvDRV_UART_INSTANCES, 0, CONF_UART_INSTANCES_MAX_NUMBER*sizeof(drv_uart_handle_t));
	return	DRV_UART_STATUS_OK;
}

drv_uart_status_t	DRV_UART_Instance_Init(drv_uart_instance_t instance, drv_uart_config_t* config)
{
	if(prvDRV_UART_INSTANCES[instance].lock != NULL) return DRV_UART_STATUS_ERROR;

	prvDRV_UART_INSTANCES[instance].lock = xSemaphoreCreateMutex();

	if(prvDRV_UART_INSTANCES[instance].lock == NULL) return DRV_UART_STATUS_ERROR;

	switch(instance)
	{
	case DRV_UART_INSTANCE_4:
		prvDRV_UART_INSTANCES[instance].deviceHandler.Instance = UART4;
		break;
	}

	/*TODO: Only baudrate is configurable*/
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.BaudRate = config->baudRate;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.WordLength = UART_WORDLENGTH_8B;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.StopBits = UART_STOPBITS_1;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.Parity = UART_PARITY_NONE;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.Mode = UART_MODE_TX_RX;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.OverSampling = UART_OVERSAMPLING_16;
	prvDRV_UART_INSTANCES[instance].deviceHandler.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	//prvDRV_UART_INSTANCES[instance].deviceHandler.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	prvDRV_UART_INSTANCES[instance].deviceHandler.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

	if (HAL_UART_Init(&prvDRV_UART_INSTANCES[instance].deviceHandler) != HAL_OK) return DRV_UART_STATUS_ERROR;
//	if (HAL_UARTEx_SetTxFifoThreshold(&prvDRV_UART_INSTANCES[instance].deviceHandler, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) return DRV_UART_STATUS_ERROR;
//	if (HAL_UARTEx_SetRxFifoThreshold(&prvDRV_UART_INSTANCES[instance].deviceHandler, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) return DRV_UART_STATUS_ERROR;
//	if (HAL_UARTEx_DisableFifoMode(&prvDRV_UART_INSTANCES[instance].deviceHandler) != HAL_OK) return DRV_UART_STATUS_ERROR;

	prvDRV_UART_INSTANCES[instance].initState = DRV_UART_INITIALIZATION_STATUS_INIT;

	return	DRV_UART_STATUS_OK;
}

drv_uart_status_t	DRV_UART_TransferData(drv_uart_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout)
{
	if(prvDRV_UART_INSTANCES[instance].initState != DRV_UART_INITIALIZATION_STATUS_INIT || prvDRV_UART_INSTANCES[instance].lock == NULL) return DRV_UART_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_UART_INSTANCES[instance].lock, pdMS_TO_TICKS(timeout)) != pdTRUE) return DRV_UART_STATUS_ERROR;

	if(HAL_UART_Transmit(&prvDRV_UART_INSTANCES[instance].deviceHandler, buffer, size, timeout) != HAL_OK) return DRV_UART_STATUS_ERROR;

	if(xSemaphoreGive(prvDRV_UART_INSTANCES[instance].lock) != pdTRUE) return DRV_UART_STATUS_ERROR;

	return	DRV_UART_STATUS_OK;
}

drv_uart_status_t	DRV_UART_Instance_RegisterRxCallback(drv_uart_instance_t instance, drv_uart_rx_isr_callback rxcb)
{
	prvDRV_UART_CALLBACKS[instance] = rxcb;

	if(HAL_UART_Receive_IT(&prvDRV_UART_INSTANCES[instance].deviceHandler, &data, 1) != HAL_OK) return DRV_UART_STATUS_ERROR;

	return	DRV_UART_STATUS_OK;

}

/**
 * @}
 */

/**
 * @}
 */
