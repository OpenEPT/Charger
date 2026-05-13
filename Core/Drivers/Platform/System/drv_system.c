/**
 ******************************************************************************
 * @file    drv_system.c
 *
 * @brief   System driver implementation providing hardware abstraction layer for
 *          STM32L476 system initialization and management. This driver handles
 *          core system setup including clock initialization and driver subsystem
 *          initialization. It provides the foundation for all other hardware
 *          components.
 *
 *          Note: Unlike the H755 dual-core variant, the L476 is a single-core
 *          device (CM4). There is no CPU2 synchronization, no hardware semaphore
 *          (HSEM), and no instruction/data cache via SCB (the L476 has only a
 *          Flash instruction cache enabled via HAL_ICACHE or flash registers).
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#include "main.h"
#include "drv_system.h"
#include "drv_gpio.h"
#include "drv_i2c.h"
#include "drv_ain.h"
#include "drv_uart.h"
#include "drv_usb_cdc.h"
/* TODO: Include individual driver headers as they are implemented, e.g.:
 *
 * #include "drv_uart.h"
 * #include "drv_timer.h"
 *
 */

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup SYSTEM_DRIVER System Driver
 * @{
 */

/**
 * @defgroup SYSTEM_PRIVATE_FUNCTIONS System driver private functions
 * @{
 */

/**
 * @brief  Initialize system clocks
 *
 * @details Delegates to the CubeMX-generated SystemClock_Config() which
 *          configures the PLL for 80 MHz SYSCLK from HSI 16 MHz:
 *          PLLM=1, PLLN=10, PLLR=2 → 80 MHz
 *
 * @retval DRV_SYSTEM_STATUS_OK if successful, DRV_SYSTEM_STATUS_ERROR otherwise
 */
static drv_system_status_t prvDRV_SYSTEM_CLOCK_Init(void)
{

	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	*/
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
	return;
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 10;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		return;
	}

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
	{
		return;
	}
	return DRV_SYSTEM_STATUS_OK;
}

/**
 * @}
 */

/**
 * @defgroup SYSTEM_PUBLIC_FUNCTIONS System driver public functions
 * @{
 */

/**
 * @brief  Initialize core system functionality
 *
 * @details Initializes HAL, configures system clocks. On the L476 there is
 *          no dual-core boot sequence, no hardware semaphore, and no
 *          instruction/data cache management at the SCB level.
 *
 * @retval DRV_SYSTEM_STATUS_OK if successful, DRV_SYSTEM_STATUS_ERROR otherwise
 */
drv_system_status_t DRV_SYSTEM_InitCoreFunc(void)
{
	if(HAL_Init() != HAL_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(prvDRV_SYSTEM_CLOCK_Init() != DRV_SYSTEM_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;

	return DRV_SYSTEM_STATUS_OK;
}

/**
 * @brief  Initialize all system drivers
 *
 * @details Initializes all hardware driver subsystems. Add driver init calls
 *          here as each Platform driver is implemented.
 *
 * @retval DRV_SYSTEM_STATUS_OK if successful, DRV_SYSTEM_STATUS_ERROR otherwise
 */
drv_system_status_t DRV_SYSTEM_InitDrivers(void)
{
	if(DRV_GPIO_Init()  != DRV_GPIO_STATUS_OK)  return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_I2C_Init()   != DRV_I2C_STATUS_OK)   return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_AIN_Init(DRV_AIN_ADC_1, NULL) != DRV_AIN_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_UART_Init()  != DRV_UART_STATUS_OK)  return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_USB_CDC_Init()  != DRV_USB_CDC_STATUS_OK)  return DRV_SYSTEM_STATUS_ERROR;

	/* TODO: Uncomment each line as the corresponding driver is implemented.
	 *
	 *
	 *
	 * if(DRV_Timer_Init() != DRV_TIMER_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	 *
	 */

	return DRV_SYSTEM_STATUS_OK;
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
