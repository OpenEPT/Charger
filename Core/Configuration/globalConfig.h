/**
 ******************************************************************************
 * @file   	globalConfig.h
 *
 * @brief  	All Global configuration macros are declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2023
 ******************************************************************************
 */

#ifndef CORE_CONFIGURATION_GLOBALCONFIG_H_
#define CORE_CONFIGURATION_GLOBALCONFIG_H_


/**
 * @defgroup CONFIGURATION System Configuration
 * @{
 */

/**
 * @defgroup GLOBALCONFIG_CONFIG Global System Configuration
 * @{
 */

/**
 * @defgroup GLOBALCONFIG_PUBLIC_DEFINES Global System Configuration public defines
 * @{
 */

/* Driver layer configuration */
/* Timer configuration */
#define CONF_DRV_TIMER_MAX_NUMBER_OF_TIMERS 		3
#define CONF_DRV_TIMER_MAX_NUMBER_OF_CHANNELS 		4

/* GPIO configuration */
#define CONF_GPIO_PORT_MAX_NUMBER					8
#define CONF_GPIO_PIN_MAX_NUMBER					16
#define	CONF_GPIO_INTERRUPTS_MAX_NUMBER				15

#define	CONF_UART_INSTANCES_MAX_NUMBER				3
#define	CONF_I2C_INSTANCES_MAX_NUMBER				3

/* AnalogIN configuration (L476: 12-bit ADC, 80 MHz APB clock) */
#define CONF_AIN_MAX_BUFFER_SIZE					100
#define CONF_AIN_ADC_BUFFER_OFFSET					2
#define CONF_AIN_ADC_BUFFER_MARKER					0xA5A5
#define CONF_AIN_MAX_BUFFER_NO						2
#define CONF_DRV_AIN_ADC_TIM_INPUT_CLK				80000000  /* Hz - APB2 @ 80 MHz */

/* AnalogOUT configuration */
#define CONF_AOUT_MAX_CHANNELS						2

/* Middleware layer configuration */

/* System service */
#define CONF_SYSTEM_TASK_NAME						"System task"
#define CONF_SYSTEM_TASK_PRIO						5
#define CONF_SYSTEM_TASK_STACK_SIZE					256
#define CONF_SYSTEM_DEFAULT_DEVICE_NAME				"Charger Device"
#define CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX			50
#define CONF_SYSTEM_BUTTON_INT_PORT        			1
#define CONF_SYSTEM_BUTTON_INT_PIN         			6
#define CONF_SYSTEM_BUTTONE_INT_PRIO        		5
#define CONF_SYSTEM_EPP_PRESENT_PORT        		1
#define CONF_SYSTEM_EPP_PRESENT_PIN         		0
#define CONF_SYSTEM_EPP_PRESENT_PRIO        		5
#define CONF_SYSTEM_LED1_PORT						0
#define CONF_SYSTEM_LED1_PIN						0

/* Status LED: LD2 on NUCLEO-L476RG is PA5 */
#define	CONF_SYSTEM_ERROR_STATUS_DIODE_PORT			0	/* Port A */
#define	CONF_SYSTEM_ERROR_STATUS_DIODE_PIN			5

#define	CONF_SYSTEM_LINK_STATUS_DIODE_PORT			0	/* Port A */
#define	CONF_SYSTEM_LINK_STATUS_DIODE_PIN			5

/* Control service */
#define CONF_CONTROL_TASK_NAME						"Control Task"
#define	CONF_CONTROL_STACK_SIZE						256
#define	CONF_CONTROL_PRIO							3
#define	CONF_CONTROL_BUFFER_SIZE					32 //TODO 512 too large?s
#define	CONF_CONTROL_RESPONSE_OK_STATUS_MSG			"STATUS"
#define	CONF_CONTROL_RESPONSE_ERROR_STATUS_MSG		"ERROR"
#define	CONF_CONTROL_STATUS_LINK_MAX_NO				1
#define	CONF_CONTROL_STATUS_LINK_TASK_NAME			"Status Link Task"
#define	CONF_CONTROL_STATUS_LINK_PRIO				4
#define	CONF_CONTROL_STATUS_LINK_STACK_SIZE			512
#define	CONF_CONTROL_STATUS_LINK_MESSAGES_MAX_NO	10
/* Charger service */
#define CONF_CHARGER_TASK_NAME						"Charger Task"
#define	CONF_CHARGER_STACK_SIZE						4096
#define	CONF_CHARGER_PRIO							3

/* Logging service */
#define CONFIG_LOGGING_TASK_NAME					"LOG Task"
#define	CONFIG_LOGGING_STACK_SIZE					256
#define	CONFIG_LOGGING_PRIO							3


/**
 * @defgroup GLOBALCONFIG_CONFIGURATION_CONFIG Configuration service configuration
 * @{
 */

#define CONF_CONFIGURATION_ENABLE                   1       /*!< Enable/disable configuration service */

#define CONF_CONFIGURATION_TASK_NAME                "Configuration service" /*!< Configuration task name */
#define CONF_CONFIGURATION_TASK_PRIO                3       /*!< Configuration task priority */
#define CONF_CONFIGURATION_TASK_STACK_SIZE          1024    /*!< Configuration task stack size */

#define CONF_CONFIGURATION_MAX_PARAMS               30      /*!< Maximum number of configuration parameters */
#define CONF_CONFIGURATION_MAX_PARAM_VALUESIZE      32      /*!< Maximum configuration parameter value length */

#define CONF_CONFIGURATION_HEADER_SIZE              8       /*!< Configuration file header size */



/**
 * @}
 */
/**
 * @}
 */
/**
 * @}
 */
#endif /* CORE_CONFIGURATION_GLOBALCONFIG_H_ */
