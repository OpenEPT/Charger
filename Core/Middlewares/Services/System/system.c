/**
 ******************************************************************************
 * @file    system.c
 *
 * @brief   System service is the core service responsible for system
 *          initialization, error handling, and status reporting.
 *          It manages the status LED on the NUCLEO-L476RG board (LD2/PA5),
 *          orchestrates driver and service initialization inside the FreeRTOS
 *          task context, and starts the RTOS scheduler.
 *
 *          Services initialized here (uncomment as each is implemented):
 *          - Charger service
 *          - Logging service
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_
#define CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_

#include <stdint.h>
#include <string.h>

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "main.h"
#include "drv_system.h"
#include "logging.h"
#include "charger.h"
#include "control.h"


#include "system.h"
#include "usbd_cdc_if.h"

/**
 * @defgroup SERVICES Service
 * @{
 */

/**
 * @defgroup SYSTEM_SERVICE System service
 * @{
 */

/**
 * @defgroup SYSTEM_PRIVATE_STRUCTURES System service private structures
 * @{
 */

/**
 * @brief Structure holding system service data
 */
typedef struct
{
	system_state_t		state;		    /*!< Current state of the system service */
	SemaphoreHandle_t	initSig;	    /*!< Semaphore for signaling initialization completion */
	SemaphoreHandle_t	guard;		    /*!< Mutex for thread-safe parameter access */
	char				deviceName[CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX]; /*!< Device name storage */
}system_data_t;

/**
 * @}
 */

/**
 * @defgroup SYSTEM_PRIVATE_DATA System service private data
 * @{
 */

static system_data_t  prvSYSTEM_DATA;		/*!< System service data instance */
static TaskHandle_t   prvSYSTEM_TASK_HANDLE;/*!< System task handle */

/**
 * @}
 */

/**
 * @defgroup SYSTEM_PRIVATE_FUNCTIONS System service private functions
 * @{
 */

/**
 * @brief GPIO callback function for user button press events (PC13 on NUCLEO-L476RG)
 *
 * @param pin GPIO pin that triggered the callback
 * @retval None
 */
static void prvBUTTON_Callback(uint32_t pin)
{
	(void)pin;
	/* TODO: Handle user button event */
}

/**
 * @brief Main system service task function
 *
 * @details This task transitions through multiple states:
 *  - SYSTEM_STATE_INIT:    Initializes drivers and services, then signals
 *                          SYSTEM_Init() via initSig semaphore.
 *  - SYSTEM_STATE_SERVICE: Waits for task notifications and handles events.
 *  - SYSTEM_STATE_ERROR:   Blinks the status LED to indicate a fatal fault.
 *
 * @param argument Not used
 * @retval None
 */
static void prvSYSTEM_Task(void *argument)
{
	(void)argument;
	uint32_t notifyValue;

	for(;;)
	{
		switch(prvSYSTEM_DATA.state)
		{
		case SYSTEM_STATE_INIT:

			/* ---- Initialize all platform drivers ---- */
			if(DRV_SYSTEM_InitDrivers() != DRV_SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
//				uint8_t TxBUF[] = "HELLO\r\n";
//				uint8_t TxBUFLen = sizeof(TxBUF);
//				while(1) {
//					CDC_Transmit_FS(TxBUF, TxBUFLen);
//							  HAL_Delay(100);
//				}
			/* ---- Initialize application services ---- */
			/* TODO: Uncomment as each service is implemented.*/

			  if(LOGGING_Init(2000) != LOGGING_STATUS_OK)
			  {
			      prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
			      break;
			  }
//
//			  if(CHARGER_Init(2000) != CHARGER_STATUS_OK)
//			  {
//			      prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
//			      break;
//			  }

			  if(CONTROL_Init(2000) != CONTROL_STATUS_OK)
			  {
				  prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				  break;
			  }


			/* Copy default device name */
			memset(prvSYSTEM_DATA.deviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);
			memcpy(prvSYSTEM_DATA.deviceName, CONF_SYSTEM_DEFAULT_DEVICE_NAME,
				   strlen(CONF_SYSTEM_DEFAULT_DEVICE_NAME));

			/* Signal SYSTEM_Init() that init is complete */
			xSemaphoreGive(prvSYSTEM_DATA.initSig);

			prvSYSTEM_DATA.state = SYSTEM_STATE_SERVICE;
			break;

		case SYSTEM_STATE_SERVICE:
			/* Wait for task notifications from other services */
			xTaskNotifyWait(0x0, 0xFFFFFFFF, &notifyValue, portMAX_DELAY);
			/* TODO: Handle notification bits as services are added */
			break;

		case SYSTEM_STATE_ERROR:
			/* Blink LD2 (PA5) to indicate system error */
			//HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
			vTaskDelay(pdMS_TO_TICKS(200));
			break;
		}
	}
}

/**
 * @}
 */

/**
 * @defgroup SYSTEM_PUBLIC_FUNCTIONS System service public functions
 * @{
 */

/**
 * @brief Initialize the system service
 *
 * @details Creates the system task, the init-complete semaphore, and the
 *          guard mutex. Blocks until the task finishes hardware init and
 *          signals via initSig.
 *
 * @retval SYSTEM_STATUS_OK if successful, SYSTEM_STATUS_ERROR otherwise
 */
system_status_t SYSTEM_Init(void)
{
	prvSYSTEM_DATA.state = SYSTEM_STATE_INIT;

	if(xTaskCreate(prvSYSTEM_Task,
			SYSTEM_TASK_NAME,
			SYSTEM_TASK_STACK_SIZE,
			NULL,
			SYSTEM_TASK_PRIO,
			&prvSYSTEM_TASK_HANDLE) != pdTRUE) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.initSig = xSemaphoreCreateBinary();
	if(prvSYSTEM_DATA.initSig == NULL) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.guard = xSemaphoreCreateMutex();
	if(prvSYSTEM_DATA.guard == NULL) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

/**
 * @brief Start the system service
 *
 * @details Initializes the CMSIS-RTOS kernel and starts the scheduler.
 *          This function does not return under normal operation.
 *
 * @retval SYSTEM_STATUS_ERROR if scheduler could not be started
 */
system_status_t SYSTEM_Start(void)
{
	if(osKernelInitialize() != osOK) return SYSTEM_STATUS_ERROR;
	if(osKernelStart() != osOK) return SYSTEM_STATUS_ERROR;
	/* Never reaches here */
	return SYSTEM_STATUS_ERROR;
}

/**
 * @brief Report system error with specified severity level
 *
 * @details Turns on LD2 on the NUCLEO-L476RG board to signal an error.
 *          All severity levels drive the same physical LED for simplicity;
 *          extend with a buzzer or additional LEDs as needed.
 *
 * @param errorLevel Error severity level. See ::system_error_level_t
 * @retval SYSTEM_STATUS_OK
 */
system_status_t SYSTEM_ReportError(system_error_level_t errorLevel)
{
	(void)errorLevel;
	/* Drive LD2 high to indicate error */
	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
	return SYSTEM_STATUS_OK;
}

/**
 * @brief Set the device name
 *
 * @param deviceName Null-terminated string containing the device name
 * @retval SYSTEM_STATUS_OK if successful, SYSTEM_STATUS_ERROR otherwise
 */
system_status_t SYSTEM_SetDeviceName(const char* deviceName)
{
	if(strlen(deviceName) > CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX) return SYSTEM_STATUS_ERROR;
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memset(prvSYSTEM_DATA.deviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);
	memcpy(prvSYSTEM_DATA.deviceName, deviceName, strlen(deviceName));

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;
	return SYSTEM_STATUS_OK;
}

/**
 * @brief Get the current device name
 *
 * @param deviceName Pointer to buffer that will receive the device name
 * @param deviceNameSize Pointer to size variable; updated with actual name length
 * @retval SYSTEM_STATUS_OK if successful, SYSTEM_STATUS_ERROR otherwise
 */
system_status_t SYSTEM_GetDeviceName(char* deviceName, uint32_t* deviceNameSize)
{
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memcpy(deviceName, prvSYSTEM_DATA.deviceName, strlen(prvSYSTEM_DATA.deviceName));
	*deviceNameSize = strlen(prvSYSTEM_DATA.deviceName);

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;
	return SYSTEM_STATUS_OK;
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

#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_ */
