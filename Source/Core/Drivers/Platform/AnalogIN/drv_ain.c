/**
 ******************************************************************************
 * @file    drv_ain.c
 *
 * @brief   Analog Input (AIN) driver implementation providing hardware abstraction 
 *          layer for STM32 ADC peripherals and external ADS9224R ADC. This driver 
 *          supports ADC initialization, multi-channel configuration, DMA-based 
 *          continuous sampling, buffer management, and callback mechanisms for 
 *          real-time signal processing. It enables high-performance analog data 
 *          acquisition with configurable resolution, sampling rate, and averaging 
 *          for various applications including sensor readings and signal analysis.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2022
 ******************************************************************************
 */
#include "drv_ain.h"
//#include "drv_gpio.h"
#include "string.h"
#include "main.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup AIN_DRIVER Analog Input Driver
 * @{
 */

/**
 * @defgroup AIN_PRIVATE_DATA AIN driver private data
 * @{
 */

static ADC_HandleTypeDef 				prvDRV_AIN_DEVICE_ADC_HANDLER;           /**< ADC peripheral handler */
static DMA_HandleTypeDef 				prvDRV_AIN_DEVICE_DMA_HANDLER;           /**< DMA handler for ADC data transfer */
static TIM_HandleTypeDef 				prvDRV_AIN_DEVICE_TIMER_HANDLER;         /**< Timer handler for ADC triggering */
static drv_ain_adc_acquisition_status_t	prvDRV_AIN_ACQUISITION_STATUS;          /**< Current ADC acquisition status */
static ADC_ChannelConfTypeDef 			prvDRV_AIN_ADC_CHANNEL_1_CONFIG;         /**< Configuration for ADC channel 1 */

static drv_ain_adc_config_t				prvDRV_AIN_ADC_CONFIG;                   /**< ADC configuration parameters */
static drv_ain_adc_stream_callback		prvDRV_AIN_ADC_CALLBACK;                 /**< Callback for ADC stream data */

static uint16_t							prvDRV_AIN_ADC_DATA_SAMPLES[CONF_AIN_MAX_BUFFER_NO][DRV_AIN_ADC_BUFFER_MAX_SIZE+DRV_AIN_ADC_BUFFER_OFFSET]
																							__attribute__((section(".ADCSamplesBuffer")));  /**< ADC samples buffer for internal ADC */
static uint8_t							prvDRV_AIN_ADC_DATA_SAMPLES_ACTIVE[CONF_AIN_MAX_BUFFER_NO];  /**< Flags indicating active buffer status */

static uint8_t							prvDRV_AIN_ADC_ACTIVE_BUFFER;    /**< Currently active buffer index */
static uint32_t							prvDRV_AIN_ADC_BUFFER_COUNTER;   /**< Buffer counter for sequential numbering */

static uint8_t							prvDRV_AIN_CAPTURE_EVENT;        /**< Flag for capture event trigger */

static uint8_t							prvDRV_AIN_CAPTURE_SINGLE_BUFFER; /**< Flag for single buffer capture mode */

static uint8_t							prvDRV_AIN_SAMPLES_NO;           /**< Number of samples per acquisition */


/**
 * @}
 */

//
static void prvDRV_AIN_BufferReady(uint8_t bufIdx);

void ADC3_IRQHandler(void)
{
	HAL_ADC_IRQHandler(&prvDRV_AIN_DEVICE_ADC_HANDLER);
}


void TIM1_UP_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&prvDRV_AIN_DEVICE_TIMER_HANDLER);
	//HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_15);
}
void TIM2_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&prvDRV_AIN_DEVICE_TIMER_HANDLER);
	//HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_15);
}

void TIM1_CC_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&prvDRV_AIN_DEVICE_TIMER_HANDLER);
}

void DMA1_Stream0_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&prvDRV_AIN_DEVICE_DMA_HANDLER);
	//DRV_GPIO_Pin_ToogleFromISR(DRV_GPIO_PORT_E, 15);

	/*Enable for debugging purposes*/
	//ITM_SendChar('a');
}

//void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
//{
//	if(hadc->Instance == ADC1)
//		prvDRV_AIN_BufferReady(0);
//}
//
//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
//{
//	if(hadc->Instance == ADC1)
//		prvDRV_AIN_BufferReady(1);
//}


void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
	if(hadc->Instance==ADC1)
	{
		GPIO_InitTypeDef GPIO_InitStruct = {0};

//		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
//		PeriphClkInitStruct.PLL2.PLL2M = 4;
//		PeriphClkInitStruct.PLL2.PLL2N = 10;
//		PeriphClkInitStruct.PLL2.PLL2P = 2;
//		PeriphClkInitStruct.PLL2.PLL2Q = 2;
//		PeriphClkInitStruct.PLL2.PLL2R = 2;
//		PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
//		PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
//		PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
//		PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
//		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
//		{
//		  Error_Handler();
//		}

		/* Peripheral clock enable */
		__HAL_RCC_ADC_CLK_ENABLE();

		__HAL_RCC_GPIOC_CLK_ENABLE();
		/**ADC1 GPIO Configuration
		PC2     ------> ADC1_INP0
		PC3     ------> ADC1_INP1
		*/
		GPIO_InitStruct.Pin  = GPIO_PIN_2 | GPIO_PIN_3;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
		/* ADC3 DMA Init */
		/* ADC3 Init */

		__HAL_RCC_DMA1_CLK_ENABLE();

		prvDRV_AIN_DEVICE_DMA_HANDLER.Instance 					= DMA1_Channel1;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.Request 				= DMA_REQUEST_0;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.Direction			= DMA_PERIPH_TO_MEMORY;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.PeriphInc 			= DMA_PINC_DISABLE;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.MemInc 				= DMA_MINC_ENABLE;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.PeriphDataAlignment 	= DMA_PDATAALIGN_HALFWORD;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.MemDataAlignment 	= DMA_MDATAALIGN_HALFWORD;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.Mode 				= DMA_CIRCULAR;
		prvDRV_AIN_DEVICE_DMA_HANDLER.Init.Priority 			= DMA_PRIORITY_HIGH;
		if (HAL_DMA_Init(&prvDRV_AIN_DEVICE_DMA_HANDLER) != HAL_OK)
		{
		  return;
		}

		__HAL_LINKDMA(&prvDRV_AIN_DEVICE_ADC_HANDLER, DMA_Handle, prvDRV_AIN_DEVICE_DMA_HANDLER);

		HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
		HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

	}

}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
	if(hadc->Instance==ADC1)
	{
		__HAL_RCC_ADC_CLK_DISABLE();
		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2 | GPIO_PIN_3);
		HAL_DMA_DeInit(hadc->DMA_Handle);
		HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
	}

}
/**
 * @defgroup AIN_PRIVATE_FUNCTIONS AIN driver private functions
 * @{
 */
/**
 * @brief Initialize DMA controller for ADC
 * 
 * This function enables the DMA clock and configures the interrupt
 * for the DMA channel used by ADC. It must be called before starting
 * the ADC data transfer.
 *
 * @retval None
 */
//static void prvDRV_AIN_InitDMA(void)
//{
//
//	/* DMA controller clock enable */
//	__HAL_RCC_DMA1_CLK_ENABLE();
//
//
//
//	/* DMA interrupt init */
//	/* BDMA_Channel0_IRQn interrupt configuration */
//	HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
//	HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
//
//}

//static void prvDRV_AIN_BufferReady(uint8_t bufIdx)
//{
//	if(prvDRV_AIN_ADC_ACTIVE_BUFFER[bufIdx] == 1)
//	{
//		DRV_AIN_Stop(DRV_AIN_ADC_1);
//		return;
//	}
//
//	memcpy(&prvDRV_AIN_ADC_DATA_SAMPLES[bufIdx][0], &prvDRV_AIN_ADC_BUFFER_COUNTER, 4);
//	prvDRV_AIN_ADC_DATA_SAMPLES[bufIdx][2] = DRV_AIN_ADC_BUFFER_MARKER;
//
//	if(prvDRV_AIN_ADC_CALLBACK != NULL)
//	{
//		prvDRV_AIN_ADC_CALLBACK(
//				(uint32_t)&prvDRV_AIN_ADC_DATA_SAMPLES[bufIdx][0],
//				bufIdx,
//				(uint32_t)prvDRV_AIN_SAMPLES_NO * sizeof(uint16_t));
//	}
//	prvDRV_AIN_ADC_ACTIVE_BUFFER[bufIdx] = 1;
//	prvDRV_AIN_ADC_BUFFER_COUNTER++;
//}
/**
 * @brief DMA half-completed callback
 * 
 * This function is called by the HAL when half of the DMA buffer has been filled.
 * It processes the filled half of the buffer, sets markers and counter values,
 * triggers the user callback if registered, and manages the buffer state.
 *
 * @param _hdma Pointer to the DMA handle
 */
static void							prvDRV_AIN_DMAHalfComplitedCallback(DMA_HandleTypeDef *_hdma)
{
//	HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_15);
//	if(prvDRV_AIN_ADC_DATA_SAMPLES_ACTIVE[prvDRV_AIN_ADC_ACTIVE_BUFFER] == 1)
//	{
//		//If we ends here, previous buffer not processed (submitted)
//		DRV_AIN_Stop(DRV_AIN_ADC_3);
//		return;
//	}
//	/* Set buffer counter */
//	memcpy(&prvDRV_AIN_ADC_DATA_SAMPLES[prvDRV_AIN_ADC_ACTIVE_BUFFER][0], &prvDRV_AIN_ADC_BUFFER_COUNTER, 4);
//
//
//	/* Set buffer marker */
//	prvDRV_AIN_ADC_DATA_SAMPLES[prvDRV_AIN_ADC_ACTIVE_BUFFER][2] = DRV_AIN_ADC_BUFFER_MARKER;
//
//	if(prvDRV_AIN_CAPTURE_EVENT == 1)
//	{
//		prvDRV_AIN_ADC_DATA_SAMPLES[prvDRV_AIN_ADC_ACTIVE_BUFFER][3] = 1;
//		prvDRV_AIN_CAPTURE_EVENT = 0;
//	}
//	else
//	{
//		prvDRV_AIN_ADC_DATA_SAMPLES[prvDRV_AIN_ADC_ACTIVE_BUFFER][3] = 0;
//	}
//
//	if(prvDRV_AIN_ADC_CALLBACK != 0 && prvDRV_AIN_CAPTURE_SINGLE_BUFFER != 1)
//	{
//		prvDRV_AIN_ADC_CALLBACK((uint32_t)&prvDRV_AIN_ADC_DATA_SAMPLES[prvDRV_AIN_ADC_ACTIVE_BUFFER][0],
//				prvDRV_AIN_ADC_ACTIVE_BUFFER,
//				prvDRV_AIN_SAMPLES_NO*2*2); //*2 for two channels and *2 because each sample is uint16_t (2 bytes)
//	}
//
//	if(prvDRV_AIN_CAPTURE_SINGLE_BUFFER == 1)
//	{
//		prvDRV_AIN_CAPTURE_SINGLE_BUFFER = 2;
//	}
//	/*Buffer is under processing*/
//	prvDRV_AIN_ADC_DATA_SAMPLES_ACTIVE[prvDRV_AIN_ADC_ACTIVE_BUFFER] = 1;
//
//	/*Increase to point to the next buffer*/
//	prvDRV_AIN_ADC_ACTIVE_BUFFER += 1;
//	prvDRV_AIN_ADC_BUFFER_COUNTER += 1;
//	prvDRV_AIN_ADC_ACTIVE_BUFFER = prvDRV_AIN_ADC_ACTIVE_BUFFER == CONF_AIN_MAX_BUFFER_NO ? 0 : prvDRV_AIN_ADC_ACTIVE_BUFFER;
}

/**
 * @brief Initialize the timer used for ADC triggering
 * 
 * This function configures the timer that will trigger ADC conversions at a specific rate.
 * It sets up timer parameters including prescaler, period, counter mode, and master configuration
 * for synchronization with the ADC.
 *
 * @return DRV_AIN_STATUS_OK if successful, DRV_AIN_STATUS_ERROR otherwise
 */
static drv_ain_status				prvDRV_AIN_InitDeviceTimer()
{

//	TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
//	TIM_MasterConfigTypeDef sMasterConfig = {0};
//
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Instance = TIM2;
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.Prescaler = 49999;
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.CounterMode = TIM_COUNTERMODE_UP;
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.Period = 3999; // 1us * 1000 = 1000ns / timPeriod [ns]
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.RepetitionCounter = 0;
//	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
//	if (HAL_TIM_Base_Init(&prvDRV_AIN_DEVICE_TIMER_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
//	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
//	if (HAL_TIM_ConfigClockSource(&prvDRV_AIN_DEVICE_TIMER_HANDLER, &sClockSourceConfig) != HAL_OK) return DRV_AIN_STATUS_ERROR;
//	if (HAL_TIM_OC_Init(&prvDRV_AIN_DEVICE_TIMER_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
//	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
//	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_UPDATE;
//	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
//	if (HAL_TIMEx_MasterConfigSynchronization(&prvDRV_AIN_DEVICE_TIMER_HANDLER, &sMasterConfig) != HAL_OK) return DRV_AIN_STATUS_ERROR;
//	return DRV_AIN_STATUS_OK;
}
/**
 * @brief Initialize the ADC device
 * 
 * This function configures the ADC peripheral with specific parameters for 
 * resolution, clock prescaler, trigger source, and DMA mode. It also configures
 * two ADC channels with appropriate sampling times and conversion modes.
 *
 * @return DRV_AIN_STATUS_OK if successful, DRV_AIN_STATUS_ERROR otherwise
 */
static drv_ain_status				prvDRV_AIN_InitDeviceADC()
{
	memset(&prvDRV_AIN_ADC_CHANNEL_1_CONFIG, 0, sizeof(ADC_ChannelConfTypeDef));

	prvDRV_AIN_DEVICE_ADC_HANDLER.Instance 						= ADC1;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler 			= ADC_CLOCK_SYNC_PCLK_DIV4;//ADC_CLOCK_ASYNC_DIV1;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Resolution				= ADC_RESOLUTION_12B;
	prvDRV_AIN_ADC_CONFIG.resolution							= DRV_AIN_ADC_RESOLUTION_12BIT;
	prvDRV_AIN_ADC_CONFIG.clockDiv								= DRV_AIN_ADC_CLOCK_DIV_1;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ScanConvMode 			= ADC_SCAN_ENABLE;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.EOCSelection 			= ADC_EOC_SEQ_CONV;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.LowPowerAutoWait 		= DISABLE;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ContinuousConvMode 		= DISABLE;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.NbrOfConversion 			= 2;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.DiscontinuousConvMode 	= DISABLE;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ExternalTrigConv 		= ADC_EXTERNALTRIG_T2_TRGO;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ExternalTrigConvEdge 	= ADC_EXTERNALTRIGCONVEDGE_RISING;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.DMAContinuousRequests 	= ENABLE;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Overrun 					= ADC_OVR_DATA_PRESERVED;
	prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode 		= DISABLE;
	if (HAL_ADC_Init(&prvDRV_AIN_DEVICE_ADC_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;

	/** Configure Regular Channel 1 */
	prvDRV_AIN_ADC_CHANNEL_1_CONFIG.Channel 					= ADC_CHANNEL_3;
	prvDRV_AIN_ADC_CHANNEL_1_CONFIG.Rank 						= ADC_REGULAR_RANK_1;
	prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime 				= DRV_AIN_ADC_SAMPLE_TIME_2C5;
	prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SingleDiff 					= ADC_DIFFERENTIAL_ENDED;
	prvDRV_AIN_ADC_CHANNEL_1_CONFIG.OffsetNumber 				= ADC_OFFSET_NONE;
	prvDRV_AIN_ADC_CHANNEL_1_CONFIG.Offset 						= 0;
	prvDRV_AIN_ADC_CONFIG.ch1.sampleTime						= DRV_AIN_ADC_SAMPLE_TIME_2C5;
	if (HAL_ADC_ConfigChannel(&prvDRV_AIN_DEVICE_ADC_HANDLER, &prvDRV_AIN_ADC_CHANNEL_1_CONFIG) != HAL_OK) return DRV_AIN_STATUS_ERROR;

	//if (HAL_ADC_RegisterCallback(&prvDRV_AIN_DEVICE_ADC_HANDLER, HAL_ADC_CONVERSION_COMPLETE_CB_ID, prvDRV_AIN_DMAHalfComplitedCallback)!= HAL_OK)return DRV_AIN_STATUS_ERROR;
	//if (HAL_ADC_RegisterCallback(&prvDRV_AIN_DEVICE_ADC_HANDLER, HAL_ADC_CONVERSION_HALF_CB_ID, prvDRV_AIN_DMAHalfComplitedCallback)!= HAL_OK)return DRV_AIN_STATUS_ERROR;
	//if(HAL_DMA_RegisterCallback(&prvDRV_AIN_DEVICE_DMA_HANDLER, HAL_DMA_XFER_CPLT_CB_ID, prvDRV_AIN_DMAHalfComplitedCallback))return DRV_AIN_STATUS_ERROR;
	//if(HAL_DMA_RegisterCallback(&prvDRV_AIN_DEVICE_DMA_HANDLER, HAL_DMA_XFER_M1CPLT_CB_ID, prvDRV_AIN_DMAHalfComplitedCallback))return DRV_AIN_STATUS_ERROR;
	return DRV_AIN_STATUS_OK;
}

/**
 * @}
 */
drv_ain_status 						DRV_AIN_Init(drv_ain_adc_t adc, drv_ain_adc_config_t* configuration)
{
	/* Initialize pin that will be used for signaling end of conversion */
//	GPIO_InitTypeDef GPIO_InitStruct = {0};
//	GPIO_InitStruct.Pin  = GPIO_PIN_15;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//    __HAL_RCC_GPIOE_CLK_ENABLE();
//    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
//    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_SET);

    /* Initialize configuration structure */
    prvDRV_AIN_ADC_CONFIG.clockDiv 		= DRV_AIN_ADC_CLOCK_DIV_UNKNOWN;
    prvDRV_AIN_ADC_CONFIG.resolution 	= DRV_AIN_ADC_RESOLUTION_UNKNOWN;
    prvDRV_AIN_ADC_CONFIG.ch1.channel	= 1;
    prvDRV_AIN_ADC_CONFIG.ch1.sampleTime= DRV_AIN_ADC_SAMPLE_TIME_UNKNOWN;
    prvDRV_AIN_ADC_CONFIG.ch1.channel	= 2;
    prvDRV_AIN_ADC_CONFIG.ch1.sampleTime= DRV_AIN_ADC_SAMPLE_TIME_UNKNOWN;
    prvDRV_AIN_ADC_CONFIG.samplingTime  = 1000;
    prvDRV_AIN_SAMPLES_NO               = DRV_AIN_ADC_BUFFER_MAX_SIZE / 2;


    /* Initialize DMA */
	//prvDRV_AIN_InitDMA();

    /* Initialize ADC */
    prvDRV_AIN_InitDeviceADC();

    /* Initialize Time */
	prvDRV_AIN_InitDeviceTimer();

	/* Set initial acquisition status*/
	prvDRV_AIN_ACQUISITION_STATUS 	 = DRV_AIN_ADC_ACQUISITION_STATUS_UNKNOWN;
	prvDRV_AIN_ADC_CALLBACK		  	 = 0;
	prvDRV_AIN_ADC_ACTIVE_BUFFER	 = 0;
	prvDRV_AIN_ADC_BUFFER_COUNTER	 = 0;
	prvDRV_AIN_CAPTURE_EVENT		 = 0;
	prvDRV_AIN_CAPTURE_SINGLE_BUFFER = 0;

	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_Start(drv_ain_adc_t adc)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;

	memset((void*)prvDRV_AIN_ADC_DATA_SAMPLES, 0, 2*(CONF_AIN_MAX_BUFFER_NO*(DRV_AIN_ADC_BUFFER_MAX_SIZE + DRV_AIN_ADC_BUFFER_OFFSET)));
	prvDRV_AIN_CAPTURE_SINGLE_BUFFER = 0;
	prvDRV_AIN_ADC_ACTIVE_BUFFER	= 0;
	prvDRV_AIN_ADC_BUFFER_COUNTER	= 0;

	//HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_RESET);

	while(HAL_ADCEx_Calibration_Start(&prvDRV_AIN_DEVICE_ADC_HANDLER, ADC_DIFFERENTIAL_ENDED) != HAL_OK);
	if(HAL_TIM_Base_Start(&prvDRV_AIN_DEVICE_TIMER_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
	if(HAL_ADC_Start_DMA(&prvDRV_AIN_DEVICE_ADC_HANDLER,
			(uint32_t*)(&prvDRV_AIN_ADC_DATA_SAMPLES[0][DRV_AIN_ADC_BUFFER_OFFSET]),
			/*(uint32_t*)(&prvDRV_AIN_ADC_DATA_SAMPLES[DRV_AIN_ADC_BUFFER_NO-1][DRV_AIN_ADC_BUFFER_OFFSET]),*/
			prvDRV_AIN_SAMPLES_NO*2)!= HAL_OK) return DRV_AIN_STATUS_ERROR;

	prvDRV_AIN_ACQUISITION_STATUS = DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE;
	return DRV_AIN_STATUS_OK;
}
drv_ain_status						DRV_AIN_SetSamplesNo(drv_ain_adc_t adc, uint32_t samplesNo)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;
	if(samplesNo > DRV_AIN_ADC_BUFFER_MAX_SIZE/2) return DRV_AIN_STATUS_ERROR;
	prvDRV_AIN_SAMPLES_NO = samplesNo;
	return DRV_AIN_STATUS_OK;
}
drv_ain_status						DRV_AIN_GetADCValue(drv_ain_adc_t adc, drv_ain_adc_channel_t channel, uint32_t* value)
{
	prvDRV_AIN_CAPTURE_SINGLE_BUFFER = 1;
	uint32_t value1[2];
	uint32_t tmpValue1[2];
	//prvDRV_AIN_InitDMA();
	DRV_AIN_Start(adc);
	while(prvDRV_AIN_CAPTURE_SINGLE_BUFFER != 2);
	DRV_AIN_Stop(adc);
	/*find active buffer*/
	uint8_t activeBufferNo = 0;

	while(activeBufferNo < DRV_AIN_ADC_BUFFER_NO)
	{
		if(prvDRV_AIN_ADC_DATA_SAMPLES_ACTIVE[activeBufferNo] == 1) break;
		activeBufferNo += 1;
	}
	DRV_AIN_Stream_SubmitAddr(adc, 0, activeBufferNo);

	if(activeBufferNo == DRV_AIN_ADC_BUFFER_NO) return DRV_AIN_STATUS_ERROR;
	tmpValue1[0] = 0;
	tmpValue1[1] = 0;
	for(uint32_t i = CONF_AIN_ADC_BUFFER_OFFSET; i < DRV_AIN_ADC_BUFFER_MAX_SIZE + CONF_AIN_ADC_BUFFER_OFFSET; i+=2)
	{
		tmpValue1[0] += prvDRV_AIN_ADC_DATA_SAMPLES[activeBufferNo][i];
		tmpValue1[1] += prvDRV_AIN_ADC_DATA_SAMPLES[activeBufferNo][i+1];
	}
	value1[0] = tmpValue1[0] / (DRV_AIN_ADC_BUFFER_MAX_SIZE/2);
	value1[1] = tmpValue1[1] / (DRV_AIN_ADC_BUFFER_MAX_SIZE/2);
	//value1[i] = HAL_ADC_GetValue(&prvDRV_AIN_DEVICE_ADC_HANDLER);
	switch(channel)
	{
	case 1:
		*value = value1[0];
		break;
	case 2:
		*value = value1[1];
		break;
	default:
		return DRV_AIN_STATUS_ERROR;
	}
	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_Stop(drv_ain_adc_t adc)
{
	if(prvDRV_AIN_ACQUISITION_STATUS != DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;

//	if(HAL_TIM_Base_Stop(&prvDRV_AIN_DEVICE_TIMER_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
//	if(HAL_ADC_Stop_DMA(&prvDRV_AIN_DEVICE_ADC_HANDLER)) return DRV_AIN_STATUS_ERROR;

	prvDRV_AIN_ACQUISITION_STATUS = DRV_AIN_ADC_ACQUISITION_STATUS_INACTIVE;
	prvDRV_AIN_CAPTURE_SINGLE_BUFFER = 0;
	return DRV_AIN_STATUS_OK;
}
drv_ain_adc_acquisition_status_t 	DRV_AIN_GetAcquisitonStatus(drv_ain_adc_t adc)
{
	return prvDRV_AIN_ACQUISITION_STATUS;
}
drv_ain_status 						DRV_AIN_SetResolution(drv_ain_adc_t adc, drv_ain_adc_resolution_t res)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;
	switch(res)
	{
	case DRV_AIN_ADC_RESOLUTION_6BIT:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Resolution = ADC_RESOLUTION_6B;
		prvDRV_AIN_ADC_CONFIG.resolution = DRV_AIN_ADC_RESOLUTION_6BIT;
		break;
	case DRV_AIN_ADC_RESOLUTION_8BIT:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Resolution = ADC_RESOLUTION_8B;
		prvDRV_AIN_ADC_CONFIG.resolution = DRV_AIN_ADC_RESOLUTION_8BIT;
		break;
	case DRV_AIN_ADC_RESOLUTION_10BIT:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Resolution = ADC_RESOLUTION_10B;
		prvDRV_AIN_ADC_CONFIG.resolution = DRV_AIN_ADC_RESOLUTION_10BIT;
		break;
	case DRV_AIN_ADC_RESOLUTION_12BIT:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Resolution = ADC_RESOLUTION_12B;
		prvDRV_AIN_ADC_CONFIG.resolution = DRV_AIN_ADC_RESOLUTION_12BIT;
		break;
	default:
		prvDRV_AIN_ADC_CONFIG.resolution = DRV_AIN_ADC_RESOLUTION_UNKNOWN;
		return DRV_AIN_STATUS_ERROR;
	}
	if (HAL_ADC_Init(&prvDRV_AIN_DEVICE_ADC_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_SetClockDiv(drv_ain_adc_t adc, drv_ain_adc_clock_div_t div)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;
	switch(div)
	{
	case DRV_AIN_ADC_CLOCK_DIV_1:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_1;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_2:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_2;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_4:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_4;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_8:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_8;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_16:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV16;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_16;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_32:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV32;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_32;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_64:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV64;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_64;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_128:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV128;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_128;
		break;
	case DRV_AIN_ADC_CLOCK_DIV_256:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV256;
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_256;
		break;
	default:
		prvDRV_AIN_ADC_CONFIG.clockDiv = DRV_AIN_ADC_CLOCK_DIV_UNKNOWN;
		break;
	}
	if (HAL_ADC_Init(&prvDRV_AIN_DEVICE_ADC_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_SetChannelsSamplingTime(drv_ain_adc_t adc, drv_ain_adc_sample_time_t stime)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;
	switch(stime)
	{
	case DRV_AIN_ADC_SAMPLE_TIME_2C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_2CYCLE_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_2C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_6C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_6CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_6C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_12C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_12C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_24C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_24CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_24C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_47C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_47C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_92C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_92C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_247C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_247C5;
		break;
	case DRV_AIN_ADC_SAMPLE_TIME_640C5:
		prvDRV_AIN_ADC_CHANNEL_1_CONFIG.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_640C5;
		break;
	default:
		prvDRV_AIN_ADC_CONFIG.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_UNKNOWN;
		return DRV_AIN_STATUS_ERROR;
	}
	if (HAL_ADC_ConfigChannel(&prvDRV_AIN_DEVICE_ADC_HANDLER, &prvDRV_AIN_ADC_CHANNEL_1_CONFIG) != HAL_OK) return DRV_AIN_STATUS_ERROR;
	return DRV_AIN_STATUS_OK;
}

drv_ain_status 						DRV_AIN_SetChannelOffset(drv_ain_adc_t adc, uint32_t channel, uint32_t offset)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;


	return DRV_AIN_STATUS_OK;
}

drv_ain_status 						DRV_AIN_SetChannelAvgRatio(drv_ain_adc_t adc, drv_adc_ch_avg_ratio_t avgRatio)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;

	switch(avgRatio)
	{
	case DRV_AIN_ADC_AVG_RATIO_UNDEFINED:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode 					= DISABLE;
		break;
	case DRV_AIN_ADC_AVG_RATIO_1:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 1;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_NONE;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_2:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 2;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_1;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_4:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 4;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_2;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_8:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 8;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_3;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_16:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 16;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_4;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_32:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 32;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_5;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_64:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 64;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_6;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_128:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 128;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_7;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	case DRV_AIN_ADC_AVG_RATIO_256:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode         			= ENABLE;      							/* Oversampling enabled */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.Ratio                 	= 256;    								/* Oversampling ratio */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.RightBitShift         	= ADC_RIGHTBITSHIFT_8;         			/* Right shift of the oversampled summation */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.TriggeredMode         	= ADC_TRIGGEREDMODE_SINGLE_TRIGGER;     /* Specifies whether or not a trigger is needed for each sample */
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.Oversampling.OversamplingStopReset 	= ADC_REGOVERSAMPLING_CONTINUED_MODE; 	/* Specifies whether or not the oversampling buffer is maintained during injection sequence */
		break;
	default:
		prvDRV_AIN_DEVICE_ADC_HANDLER.Init.OversamplingMode 					= DISABLE;
		return DRV_AIN_STATUS_ERROR;
	}
	if (HAL_ADC_Init(&prvDRV_AIN_DEVICE_ADC_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_SetSamplingPeriod(drv_ain_adc_t adc, uint32_t period, uint32_t prescaller)
{
	if(prvDRV_AIN_ACQUISITION_STATUS == DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE) return DRV_AIN_STATUS_ERROR;
	prvDRV_AIN_ADC_CONFIG.samplingTime = prescaller/DRV_AIN_ADC_TIM_INPUT_CLK*period;

	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.Prescaler = prescaller;
	prvDRV_AIN_DEVICE_TIMER_HANDLER.Init.Period = period;
	if (HAL_TIM_Base_Init(&prvDRV_AIN_DEVICE_TIMER_HANDLER) != HAL_OK) return DRV_AIN_STATUS_ERROR;
	return DRV_AIN_STATUS_OK;
}
drv_ain_adc_resolution_t 			DRV_AIN_GetResolution(drv_ain_adc_t adc)
{
	return prvDRV_AIN_ADC_CONFIG.resolution;
}
drv_ain_adc_sample_time_t 			DRV_AIN_GetSamplingTime(drv_ain_adc_t adc, drv_ain_adc_channel_t channel)
{
	return prvDRV_AIN_ADC_CONFIG.ch1.sampleTime;
}
drv_ain_status 						DRV_AIN_GetADCClk(drv_ain_adc_t adc, uint32_t *clk)
{
	*clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ADC);
	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_Stream_RegisterCallback(drv_ain_adc_t adc, drv_ain_adc_stream_callback cbfunction)
{
	prvDRV_AIN_ADC_CALLBACK = cbfunction;
	return DRV_AIN_STATUS_OK;
}
drv_ain_status 						DRV_AIN_Stream_SubmitAddr(drv_ain_adc_t adc, uint32_t addr, uint8_t bufferID)
{
	if(bufferID >= DRV_AIN_ADC_BUFFER_NO) return DRV_AIN_STATUS_ERROR;
	//TODO: Mutual exclusion, protect it by disabling ADC Interrupt;
	prvDRV_AIN_ADC_DATA_SAMPLES_ACTIVE[bufferID] = 0;
	return DRV_AIN_STATUS_OK;
}

drv_ain_status 						DRV_AIN_Stream_SetCapture(uint32_t* packetCounter)
{
	//TODO: Should be protected
	prvDRV_AIN_CAPTURE_EVENT = 1;

	*packetCounter = prvDRV_AIN_ADC_BUFFER_COUNTER;

	return DRV_AIN_STATUS_OK;
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

