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
#include "configuration.h"
#include "drv_timer.h"
#include "drv_gpio.h"


#include "system.h"





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
	system_state_t				state;		    /*!< Current state of the system service */
	SemaphoreHandle_t			initSig;	    /*!< Semaphore for signaling initialization completion */
	SemaphoreHandle_t			guard;		    /*!< Mutex for thread-safe parameter access */
	char						deviceName[CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX]; /*!< Device name storage */
	system_rgb_value_t			rgbValue;      /**< Current RGB LED values */
	system_opstate_t			opState;
	charger_charging_state_t	chState;
	uint8_t						eppPresent;
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
 * @defgroup SYSTEM_DEFINES System task defines and default values
 * @{
 */
#define  SYSTEM_MASK_RGB_SET_COLOR			0x00000001  /**< Task notification flag for setting RGB LED color */
#define  SYSTEM_MASK_SET_OP_STATE			0x00000002  /**< Task notification flag for setting RGB LED color */
#define  SYSTEM_MASK_SET_CHARGING_STATE		0x00000004  /**< Task notification flag for setting RGB LED color */
/**
 * @}
 */

/**
 * @defgroup SYSTEM_PRIVATE_FUNCTIONS System service private functions
 * @{
 */

#define  SYSTEM_BUTTON_INT_PORT        		CONF_SYSTEM_BUTTON_INT_PORT     	/*!< GPIO port used for BQ25180 interrupt signal */
#define  SYSTEM_BUTTON_INT_PIN         		CONF_SYSTEM_BUTTON_INT_PIN       /*!< GPIO pin used for BQ25180 interrupt signal */
#define  SYSTEM_BUTTONE_INT_PRIO       		CONF_SYSTEM_BUTTONE_INT_PRIO       /*!< Interrupt priority for BQ25180 interrupt line */


#define	 SYSTEM_EPP_PRESENT_PORT        	CONF_SYSTEM_EPP_PRESENT_PORT
#define  SYSTEM_EPP_PRESENT_PIN         	CONF_SYSTEM_EPP_PRESENT_PIN

#define	 SYSTEM_LED1_PORT        			CONF_SYSTEM_LED1_PORT
#define  SYSTEM_LED1_PIN         			CONF_SYSTEM_LED1_PIN
/**
 * @brief GPIO callback function for user button press events (PC13 on NUCLEO-L476RG)
 *
 * @param pin GPIO pin that triggered the callback
 * @retval None
 */
static void prvBUTTON_Callback(uint32_t pin)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(prvSYSTEM_TASK_HANDLE, SYSTEM_MASK_SET_CHARGING_STATE, eSetBits, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/**
 * @brief Set RGB LED PWM duty cycle values
 *
 * This function updates PWM duty cycle values for RGB LED channels.
 *
 * @param red     Red channel intensity value
 * @param blue    Blue channel intensity value
 * @param green   Green channel intensity value
 *
 * @retval SYSTEM_STATUS_OK     RGB state successfully updated
 * @retval SYSTEM_STATUS_ERROR  Failed to update RGB PWM channels
 */
static system_status_t prvSYSTEM_SetRGBState(uint8_t red, uint8_t blue, uint8_t green)
{
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_2, DRV_TIMER_CHANNEL_3, red, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_2, DRV_TIMER_CHANNEL_4, green, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_2, DRV_TIMER_CHANNEL_2, blue, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	return SYSTEM_STATUS_ERROR;
}
/**
 * @brief Initialize PWM timer used for RGB LED control
 *
 * @retval SYSTEM_STATUS_OK     PWM successfully initialized
 * @retval SYSTEM_STATUS_ERROR  PWM initialization failed
 */
static system_status_t prvSYSTEM_InitPWM(void)
{
	drv_timer_channel_config_t pwmTimerChConfig;
	drv_timer_config_t         pwmTimerConfig;

	pwmTimerConfig.mode       = DRV_TIMER_COUNTER_MODE_UP;
	pwmTimerConfig.prescaler  = 2000;
	pwmTimerConfig.preload    = DRV_TIMER_PRELOAD_DISABLE;
	pwmTimerConfig.div        = DRV_TIMER_DIV_1;
	pwmTimerConfig.period     = 256;

	if(DRV_Timer_Init_Instance(DRV_TIMER_2, &pwmTimerConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	pwmTimerChConfig.mode = DRV_TIMER_CHANNEL_MODE_PWM1;

	if(DRV_Timer_Channel_Init(DRV_TIMER_2, DRV_TIMER_CHANNEL_2, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	if(DRV_Timer_Channel_Init(DRV_TIMER_2, DRV_TIMER_CHANNEL_3, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	if(DRV_Timer_Channel_Init(DRV_TIMER_2, DRV_TIMER_CHANNEL_4, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	return SYSTEM_STATUS_OK;
}

/**
 * @brief Initialize PWM timer used for RGB LED control
 *
 * @retval SYSTEM_STATUS_OK     PWM successfully initialized
 * @retval SYSTEM_STATUS_ERROR  PWM initialization failed
 */
static system_status_t prvSYSTEM_InitSystemButton(void)
{
	// Configure the pin for the button
	drv_gpio_pin_init_conf_t button_pin_conf;
	button_pin_conf.mode = DRV_GPIO_PIN_MODE_IT_FALLING;
	button_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;


	if(DRV_GPIO_Port_Init(SYSTEM_BUTTON_INT_PORT) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;

	if(DRV_GPIO_Pin_Init(SYSTEM_BUTTON_INT_PORT, SYSTEM_BUTTON_INT_PIN, &button_pin_conf) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;

	if(DRV_GPIO_RegisterCallback(SYSTEM_BUTTON_INT_PORT, SYSTEM_BUTTON_INT_PIN, prvBUTTON_Callback, SYSTEM_BUTTONE_INT_PRIO) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

static system_status_t prvSYSTEM_InitEPPPresentPin(void)
{
	// Configure the pin for the button
	drv_gpio_pin_init_conf_t button_pin_conf;
	button_pin_conf.mode = DRV_GPIO_PIN_MODE_INPUT;
	button_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;


	if(DRV_GPIO_Port_Init(SYSTEM_EPP_PRESENT_PORT) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;

	if(DRV_GPIO_Pin_Init(SYSTEM_EPP_PRESENT_PORT, SYSTEM_EPP_PRESENT_PIN, &button_pin_conf) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;



	return SYSTEM_STATUS_OK;
}
static system_status_t prvSYSTEM_InitLED1Pin(void)
{
	// Configure the pin for the button
	drv_gpio_pin_init_conf_t diode_pin_conf;
	diode_pin_conf.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	diode_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	if(DRV_GPIO_Port_Init(SYSTEM_LED1_PORT) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;

	if(DRV_GPIO_Pin_Init(SYSTEM_LED1_PORT, SYSTEM_LED1_PIN, &diode_pin_conf) != DRV_GPIO_STATUS_OK)
		return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}
/**
 * @brief Update EPP presence status
 * @retval void
 */
static void prvSYSTEM_UpdateEPPPresentStatus(void)
{
	uint8_t previousEPPPresent = prvSYSTEM_DATA.eppPresent;

	if(DRV_GPIO_Pin_ReadState(SYSTEM_EPP_PRESENT_PORT, SYSTEM_EPP_PRESENT_PIN) == DRV_GPIO_PIN_STATE_SET)
	{
		prvSYSTEM_DATA.eppPresent = 1;
		DRV_GPIO_Pin_SetState(SYSTEM_LED1_PORT, SYSTEM_LED1_PIN, GPIO_PIN_SET);
	}
	else
	{
		prvSYSTEM_DATA.eppPresent = 0;
		DRV_GPIO_Pin_SetState(SYSTEM_LED1_PORT, SYSTEM_LED1_PIN, GPIO_PIN_RESET);
	}

	if(previousEPPPresent != prvSYSTEM_DATA.eppPresent)
	{
		if(prvSYSTEM_DATA.eppPresent)
		{
			LOGGING_Write("System service", LOGGING_MSG_TYPE_INFO, "EPP connected\r\n");
		}
		else
		{
			LOGGING_Write("System service", LOGGING_MSG_TYPE_WARNING, "EPP disconnected\r\n");
		}
	}
}

static void prvSYSTEM_ChargerStatusCallback(charger_charging_status_t status)
{
    switch(status)
    {
        case CHARGER_CHARGING_STATUS_DONE:
            SYSTEM_SetOpState(SYSTEM_OPSTATE_CHARGING_DONE);
            break;

        case CHARGER_CHARGING_STATUS_CC:
            SYSTEM_SetOpState(SYSTEM_OPSTATE_CHARGING_CC);
            break;

        case CHARGER_CHARGING_STATUS_CV:
            SYSTEM_SetOpState(SYSTEM_OPSTATE_CHARGING_CV);
            break;

        default:
            break;
    }
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
	uint8_t brightness = 50;
	int8_t brightnessStep = -2;

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

			if(prvSYSTEM_InitPWM() != SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}

			if(prvSYSTEM_InitSystemButton() != SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}


			if(prvSYSTEM_InitEPPPresentPin() != SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}

			if(prvSYSTEM_InitLED1Pin() != SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}

			if(CONFIGURATION_Init(2000) != CONFIGURATION_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}

			if(LOGGING_Init(2000) != LOGGING_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}

			if(CHARGER_Init(2000) != CHARGER_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			CHARGER_RegisterStatusCallback(prvSYSTEM_ChargerStatusCallback);

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

			SYSTEM_SetOpState(SYSTEM_OPSTATE_CHARGING_DONE);

			prvSYSTEM_DATA.state = SYSTEM_STATE_SERVICE;

			break;

		case SYSTEM_STATE_SERVICE:
			xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, pdMS_TO_TICKS(100));

			prvSYSTEM_UpdateEPPPresentStatus();

			switch(prvSYSTEM_DATA.opState)
			{
			case SYSTEM_OPSTATE_ERROR:
				prvSYSTEM_SetRGBState(50, 0, 0);
				break;
			case SYSTEM_OPSTATE_CHARGING_CC:
				prvSYSTEM_SetRGBState(brightness, brightness, 0);

				if(brightness <= 2)
				{
					brightness = 2;
					brightnessStep = 2;
				}
				else if(brightness >= 50)
				{
					brightness = 50;
					brightnessStep = -2;
				}

				brightness += brightnessStep;
				break;
			case SYSTEM_OPSTATE_CHARGING_INTERRUPTED:
				prvSYSTEM_SetRGBState(0, 50, 50);
				break;
			case SYSTEM_OPSTATE_CHARGING_CV:
				prvSYSTEM_SetRGBState(brightness, 0, brightness);
				if(brightness <= 2)
				{
					brightness = 2;
					brightnessStep = 2;
				}
				else if(brightness >= 50)
				{
					brightness = 50;
					brightnessStep = -2;
				}

				brightness += brightnessStep;
				break;
			case SYSTEM_OPSTATE_CHARGING_DONE:
				prvSYSTEM_SetRGBState(50, 50, 50);
				break;
			case SYSTEM_OPSTATE_OPERATIONAL:
				prvSYSTEM_SetRGBState(0, 0, 50);
				break;
			}

			if(notifyValue == 0) continue;


			if((notifyValue & SYSTEM_MASK_RGB_SET_COLOR) != 0)
			{
				prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
				break;
			}
			if((notifyValue & SYSTEM_MASK_SET_OP_STATE) != 0)
			{

			}
			if((notifyValue & SYSTEM_MASK_SET_CHARGING_STATE) != 0)
			{
				if(prvSYSTEM_DATA.chState == CHARGER_CHARGING_ENABLE)
				{
					prvSYSTEM_DATA.chState = CHARGER_CHARGING_DISABLE;
					CHARGER_SetChargingState(CHARGER_CHARGING_DISABLE, 1000);
				}
				else
				{
					prvSYSTEM_DATA.chState = CHARGER_CHARGING_ENABLE;
					CHARGER_SetChargingState(CHARGER_CHARGING_ENABLE, 1000);
				}
			}
			break;

		case SYSTEM_STATE_ERROR:
			prvSYSTEM_DATA.opState = SYSTEM_OPSTATE_ERROR;
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
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

system_status_t SYSTEM_ReportError(system_error_level_t errorLevel)
{
	prvSYSTEM_DATA.rgbValue.red = 50;
	prvSYSTEM_DATA.rgbValue.blue = 0;
	prvSYSTEM_DATA.rgbValue.green = 0;
	switch(errorLevel)
	{
	case SYSTEM_ERROR_LEVEL_LOW:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
			break;
	case SYSTEM_ERROR_LEVEL_MEDIUM:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
		break;
	case SYSTEM_ERROR_LEVEL_HIGH:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
		break;
	}
	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_SetOpState(system_opstate_t opState)
{
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.opState = opState;

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	if(xTaskNotify(prvSYSTEM_TASK_HANDLE, SYSTEM_MASK_SET_OP_STATE, eSetBits) != pdTRUE) return SYSTEM_STATUS_ERROR;
	return SYSTEM_STATUS_OK;
}


system_status_t SYSTEM_SetRGB(system_rgb_value_t value)
{
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.rgbValue.red = value.red;
	prvSYSTEM_DATA.rgbValue.blue = value.blue;
	prvSYSTEM_DATA.rgbValue.green = value.green;

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	if(xTaskNotify(prvSYSTEM_TASK_HANDLE, SYSTEM_MASK_RGB_SET_COLOR, eSetBits) != pdTRUE) return SYSTEM_STATUS_ERROR;

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
