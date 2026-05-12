/**
 ******************************************************************************
 * @file   	drv_usb_cdc.h
 *
 * @brief  	USB CDC driver implementation for STM32L476
 *			Call flow:
 *				1. DRV_USB_CDC_Init()
 *				2. USB Host enumerates device
 *				3. Host opens COM port DTR = 1
 *				4. Transfer - send bytes to host
 *				5. Receive through interrupt
 * @author	Dimitrije Lilic
 * @email	dimitrijelilic94@yahoo.com
 * @date	April 2026
 ******************************************************************************
 */
#include "drv_usb_cdc.h"

#include "main.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "usb_device.h"
#include "usbd_cdc_if.h"



/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup USB_CDC_DRIVER USB CDC Driver
 * @{
 */

/**
 * @defgroup USB_CDC_PRIVATE_STRUCTURES USB CDC driver private structures
 * @{
 */

/**
 * @defgroup USB_CDC_PRIVATE_DEFINES USB CDC driver private defines
 * @{
 */

#define DRV_USB_CDC_TX_BUSY_RETRIES 	200U

#define DRV_USB_CDC_TX_BUSY_RETRY_DELAY	1U
/**
 * @}
 */

/**
 * @brief USB_CDC driver internal handle structure
 */
typedef struct
{

	drv_usb_cdc_initialization_status_t	initState;     	/**< USB_CDC initialization state */
	volatile uint8_t					dtrActive;   	/**< DTR line state */
	SemaphoreHandle_t					lock;          	/**< Mutex for thread-safe operations */
	drv_usb_cdc_rx_isr_callback			rxCallback;  	/**< Registered Rx callback */
}drv_usb_cdc_handle_t;
/**
 * @}
 */

/**
 * @defgroup USB_CDC_PRIVATE_DATA USB_CDC driver private data
 * @{
 */
static drv_usb_cdc_handle_t 		prvDRV_USB_CDC_HANDLE;  /**< USB_CDC handle */

/**
 * @}
 */


drv_usb_cdc_status_t	DRV_USB_CDC_Init(void)
{
	prvDRV_USB_CDC_HANDLE.initState 	= DRV_USB_CDC_INITIALIZATION_STATUS_NOINIT;
	prvDRV_USB_CDC_HANDLE.rxCallback 	= NULL;
	prvDRV_USB_CDC_HANDLE.dtrActive 	= 0;

	prvDRV_USB_CDC_HANDLE.lock = xSemaphoreCreateMutex();
	if(prvDRV_USB_CDC_HANDLE.lock == NULL) return DRV_USB_CDC_STATUS_ERROR;
	//TODO
	//MX_USB_DEVICE_Init();

	prvDRV_USB_CDC_HANDLE.initState 	= DRV_USB_CDC_INITIALIZATION_STATUS_INIT;

	return	DRV_USB_CDC_STATUS_OK;
}

drv_usb_cdc_status_t	DRV_USB_CDC_TransferData(uint8_t* buffer, uint16_t size, uint32_t timeout)
{
	if(prvDRV_USB_CDC_HANDLE.initState != DRV_USB_CDC_INITIALIZATION_STATUS_INIT ||
			prvDRV_USB_CDC_HANDLE.lock == NULL) return DRV_USB_CDC_STATUS_ERROR;

	if(!prvDRV_USB_CDC_HANDLE.dtrActive) return DRV_USB_CDC_STATUS_DISCONNECTED;

	if(xSemaphoreTake(prvDRV_USB_CDC_HANDLE.lock, pdMS_TO_TICKS(timeout)) != pdTRUE) return DRV_USB_CDC_STATUS_ERROR;

	uint8_t result 		= USBD_BUSY;
	uint32_t retries	= 0;

	while(result == USBD_BUSY && retries < DRV_USB_CDC_TX_BUSY_RETRIES)
	{
		result = CDC_Transmit_FS(buffer, size);
		if(result == USBD_BUSY)
		{
			vTaskDelay(pdMS_TO_TICKS(DRV_USB_CDC_TX_BUSY_RETRY_DELAY));
			retries++;
		}
	}

	if(xSemaphoreGive(prvDRV_USB_CDC_HANDLE.lock) != pdTRUE) return DRV_USB_CDC_STATUS_ERROR;

	if(result != USBD_OK) return DRV_USB_CDC_STATUS_ERROR;

	return	DRV_USB_CDC_STATUS_OK;
}

drv_usb_cdc_status_t	DRV_USB_CDC_RegisterRxCallback(drv_usb_cdc_rx_isr_callback rxcb)
{
	prvDRV_USB_CDC_HANDLE.rxCallback = rxcb;
	return	DRV_USB_CDC_STATUS_OK;

}

uint8_t DRV_USB_CDC_IsConnected(void)
{
	return prvDRV_USB_CDC_HANDLE.dtrActive;
}

void DRV_USB_CDC_RxDataAvailable(uint8_t* data, uint32_t len)
{
	if(prvDRV_USB_CDC_HANDLE.rxCallback != NULL) {
		prvDRV_USB_CDC_HANDLE.rxCallback(data, len);
	}
}

void DRV_USB_CDC_SetLineState(uint8_t dtr)
{
	 prvDRV_USB_CDC_HANDLE.dtrActive = dtr;
}

/**
 * @}
 */

/**
 * @}
 */
