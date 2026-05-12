/**
 ******************************************************************************
 * @file   	drv_usb_cdc.h
 *
 * @brief  	USB CDC driver provides hardware abstraction layer for STM32 USB
 * 			peripheral operating as CDC device
 * 			This driver supports USB CDC initialization, blocking transmit with
 * 			mutex, interrupt driven receive, host connection check
 * 			All USB CDC driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Dimitrije Lilic
 * @email	dimitrijelilic94@yahoo.com
 * @date	April 2026
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_USB_CDC_DRV_USB_CDC_H_
#define CORE_DRIVERS_PLATFORM_USB_CDC_DRV_USB_CDC_H_

#include <stdint.h>
#include "globalConfig.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup USB_CDC_DRIVER USB CDC Driver
 * @{
 */

/**
 * @defgroup USB_CDC_PUBLIC_TYPES USB_CDC driver public data types
 * @{
 */

/**
 * @brief USB_CDC driver return status
 */
typedef enum
{
	DRV_USB_CDC_STATUS_OK,				/*!< USB_CDC operation successful */
	DRV_USB_CDC_STATUS_ERROR,			/*!< USB_CDC operation failed */
	DRV_USB_CDC_STATUS_BUSY,			/*!< USB_CDC transmitter busy */
	DRV_USB_CDC_STATUS_DISCONNECTED,	/*!< Host not connected */
}drv_usb_cdc_status_t;

/**
 * @brief USB_CDC initialization status
 */
typedef enum
{
	DRV_USB_CDC_INITIALIZATION_STATUS_NOINIT	=	0,		/*!< USB_CDC not initialized */
	DRV_USB_CDC_INITIALIZATION_STATUS_INIT		=	1		/*!< USB_CDC initialized */
}drv_usb_cdc_initialization_status_t;

/**
 * @brief USB_CDC receive interrupt callback function pointer type
 * @param data: Received data byte
 */
typedef void (*drv_usb_cdc_rx_isr_callback)(uint8_t* data, uint32_t len);

/**
 * @}
 */

/**
 * @defgroup USB_CDC_PUBLIC_FUNCTIONS USB_CDC driver interface functions
 * @{
 */

/**
 * @brief	Initialize USB_CDC driver subsystem
 * @details	This function initializes the USB_CDC driver subsystem, sets up
 * 			internal data structures, and prepares the driver for USB_CDC
 * 			instance initialization. Must be called before any USB_CDC
 * 			instance operations.
 * @retval	::drv_usb_cdc_status_t
 */
drv_usb_cdc_status_t	DRV_USB_CDC_Init();

/**
 * @brief	Transmit data through USB_CDC instance
 * @details	This function transmits data through the specified USB_CDC instance
 * 			with timeout control. The function blocks until all data is
 * 			transmitted or timeout occurs.
 * @param	instance: USB_CDC instance for transmission. See ::drv_usb_cdc_instance_t
 * @param	buffer: Pointer to data buffer to transmit
 * @param	size: Number of bytes to transmit
 * @param	timeout: Transmission timeout in milliseconds
 * @retval	::drv_usb_cdc_status_t
 */
drv_usb_cdc_status_t	DRV_USB_CDC_TransferData(uint8_t* buffer, uint16_t size, uint32_t timeout);

/**
 * @brief	Register receive interrupt callback function
 * @details	This function registers a callback function that will be called
 * 			when data is received on the specified USB_CDC instance. The callback
 * 			is executed in interrupt context and should be kept short.
 * @param	instance: USB_CDC instance. See ::drv_usb_cdc_instance_t
 * @param	rxcb: Callback function pointer. See ::drv_usb_cdc_rx_isr_callback
 * @retval	::drv_usb_cdc_status_t
 */
drv_usb_cdc_status_t	DRV_USB_CDC_Instance_RegisterRxCallback(drv_usb_cdc_rx_isr_callback rxcb);

/**
 * @brief	Query whether a host is connected (DTR line active)
 *
 * @retval	1 if connected, 0 otherwise
 */
uint8_t					DRV_USB_CDC_IsConnected(void);

/**
 * @brief	Bridge function - call this from CDC_Recieve_FS in usbd_cdc_if.c
 *
 * @detail  Forwards received bytes to the registered RX callback
 * @data    data POinter to received bytes
 * @retval	len Number of bytes received
 */
void					DRV_USB_CDC_RxDataAvailable(uint8_t*  data, uint32_t len);

/**
 * @brief	Notify driver that DTR line state has changed
 *
 * @detail  Called from CDC_Control_FS() in usbd_cdc_if.c
 * @data    data POinter to received bytes
 * @retval	len Number of bytes received
 */
void					DRV_USB_CDC_SetLineState(uint8_t dtr);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_USB_CDC_DRV_USB_CDC_H_ */
