/**
 ******************************************************************************
 * @file   	control.c
 *
 * @brief  	Control service is in charge to receive and process control messages.
 * 			This service communicates with others services defined within this
 * 			firmware. Communications is mostly related to the configuration of
 * 			corresponding service or obtaining status messages from certain service
 * 			defined within control message content.
 * 			All control service logic is implemented within this file
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	December 2023
 ******************************************************************************
 */
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "control.h"
#include "logging.h"
#include "system.h"
//#include "sstream.h"
//#include "energy_debugger.h"
#include "CMParse/cmparse.h"
//#include "dpcontrol.h"
#include "charger.h"
#include "drv_usb_cdc.h"
#include "drv_uart.h"

/**
 * @defgroup SERVICES Service
 * @{
 */

/**
 * @defgroup CONTROL_SERVICE Control service
 * @{
 */
/**
 * @defgroup CONTROL_PRIVATE_STRUCTURES Control service private structures defines
 * @{
 */
typedef struct
{
	uint8_t				data[CONTROL_BUFFER_SIZE];
	uint32_t			length;
	control_source_t	source;
} control_rx_packet_t;

typedef struct
{
	TaskHandle_t		taskHandle;
	SemaphoreHandle_t	initSig;
	SemaphoreHandle_t	guard;
	QueueHandle_t		rxQueue;
	char				requestBuffer[CONTROL_BUFFER_SIZE];
	char				responseBuffer[CONTROL_BUFFER_SIZE];
	uint16_t			responseBufferSize;
	control_state_t		state;
}control_data_t;

typedef struct
{
	TaskHandle_t			taskHandle;
	SemaphoreHandle_t		initSig;
	SemaphoreHandle_t		guard;
	QueueHandle_t			messageQueue;
	control_state_t			state;
	control_link_state_t	linkState;
	char					messageBuffer[CONTROL_BUFFER_SIZE];
	uint32_t				messageBufferSize;
}control_status_link_data_t;

typedef struct
{
	char							message[CONTROL_BUFFER_SIZE];
	uint32_t						messageSize;
	contol_status_message_type_t	type;
}control_status_message_t;
/**
 * @}
 */
/**
 * @defgroup CONTROL_PRIVATE_DATA Control service private data instances
 * @{
 */
static control_data_t				prvCONTROL_DATA;
static control_status_link_data_t	prvCONTROL_STATUS_LINK_DATA[CONTROL_STATUS_LINK_MAX_NO];

/**
 * @brief  Line accumulator for UART4.
 *         Bytes are collected here until CR or LF is detected, then the
 *         assembled frame is pushed to rxQueue and the buffer is reset.
 */
static uint8_t  prvCONTROL_UART4_LineBuf[CONTROL_BUFFER_SIZE];
static uint32_t prvCONTROL_UART4_LineLen;
/**
 * @}
 */
/**
 * @defgroup CONTROL_PRIVATE_FUNCTIONS Control service private functions
 * @{
 */

/**
 * @brief	Prepare response in case of error
 * @param	response: buffer where response message will be stored
 * @param	responseSize: response message size
 * @retval	void
 */
static void inline prvCONTROL_PrepareErrorResponse(char* response, uint16_t* responseSize)
{
	uint32_t	tmpIncreaseSize  = 0;
	char* tmpResponsePtr = response;
	tmpIncreaseSize = strlen(CONTROL_RESPONSE_ERROR_STATUS_MSG);
	memcpy(tmpResponsePtr, CONTROL_RESPONSE_ERROR_STATUS_MSG, tmpIncreaseSize);
	tmpResponsePtr	+= tmpIncreaseSize;
	*responseSize	+= tmpIncreaseSize;

	tmpIncreaseSize = strlen(" 1");
	memcpy(tmpResponsePtr, " 1", tmpIncreaseSize);
	tmpResponsePtr	+= tmpIncreaseSize;
	*responseSize	+= tmpIncreaseSize;

	memcpy(tmpResponsePtr, "\r\n", 2);
	tmpResponsePtr	+= 2;
	*responseSize	+= 2;
}
/**
 * @brief	Prepare response in case when request is successfully process
 * @param	response: buffer where response message will be stored
 * @param	responseSize: response message size
 * @param	msg: message that will be integrated between ::CONTROL_RESPONSE_OK_STATUS_MSG and end of the message defined within "\r\n"
 * @param	msgSize: size of the ::msg
 * @param	responseSize: response message size
 * @retval	void
 */
static void inline prvCONTROL_PrepareOkResponse(char* response, uint16_t* responseSize, char* msg, uint32_t msgSize)
{
	uint32_t	tmpIncreaseSize  = 0;
	char* tmpResponsePtr = response;
	tmpIncreaseSize = strlen(CONTROL_RESPONSE_OK_STATUS_MSG);
	memcpy(tmpResponsePtr, CONTROL_RESPONSE_OK_STATUS_MSG, tmpIncreaseSize);
	tmpResponsePtr	+= tmpIncreaseSize;
	*responseSize	+= tmpIncreaseSize;

	memcpy(tmpResponsePtr, " ", 1);
	tmpResponsePtr	+= 1;
	*responseSize	+= 1;

	memcpy(tmpResponsePtr, msg, msgSize);
	tmpResponsePtr	+= msgSize;
	*responseSize	+= msgSize;

	memcpy(tmpResponsePtr, "\r\n", 2);
	tmpResponsePtr	+= 2;
	*responseSize	+= 2;
}
/**
 * @brief	Get device name from system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
static void prvCONTROL_UndefinedCommand(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	*responseSize = 0;

	prvCONTROL_PrepareErrorResponse(response, responseSize);
	return;
}
/**
 * @brief	Get device name from system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
static void prvCONTROL_GetDeviceName(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint32_t 	deviceNameSize;
	char tmpDeviceName[CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX];
	memset(tmpDeviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);

	*responseSize = 0;

	if(SYSTEM_GetDeviceName(tmpDeviceName, &deviceNameSize) != SYSTEM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	prvCONTROL_PrepareOkResponse(response, responseSize, tmpDeviceName, deviceNameSize);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Device name successfully obtained\r\n");
}
/**
 * @brief	Set device name by utilazing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
static void prvCONTROL_SetDeviceName(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t	value;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	if(SYSTEM_SetDeviceName(value.value) != SYSTEM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	prvCONTROL_PrepareOkResponse(response, responseSize, "", 0);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Device name successfully set\r\n");
}

/**
 * @brief	Set RGB color
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
static void prvCONTROL_SetRGBColor(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
//	cmparse_value_t	value;
//	uint32_t		intValue;
//	system_rgb_value_t rgbValue;
//
//	memset(&value, 0, sizeof(cmparse_value_t));
//	if(CMPARSE_GetArgValue(arguments, argumentsLength, "r", &value) != CMPARSE_STATUS_OK)
//	{
//		prvCONTROL_PrepareErrorResponse(response, responseSize);
//		return;
//	}
//	sscanf(value.value, "%lu", &intValue);
//
//	rgbValue.red = (uint8_t)intValue;
//
//	memset(&value, 0, sizeof(cmparse_value_t));
//	if(CMPARSE_GetArgValue(arguments, argumentsLength, "g", &value) != CMPARSE_STATUS_OK)
//	{
//		prvCONTROL_PrepareErrorResponse(response, responseSize);
//		return;
//	}
//	sscanf(value.value, "%lu", &intValue);
//
//	rgbValue.green = (uint8_t)intValue;
//
//	memset(&value, 0, sizeof(cmparse_value_t));
//
//	if(CMPARSE_GetArgValue(arguments, argumentsLength, "b", &value) != CMPARSE_STATUS_OK)
//	{
//		prvCONTROL_PrepareErrorResponse(response, responseSize);
//		return;
//	}
//	sscanf(value.value, "%lu", &intValue);
//
//	rgbValue.blue = (uint8_t)intValue;
//
//	if(SYSTEM_SetRGB(rgbValue) != SYSTEM_STATUS_OK)
//	{
//		prvCONTROL_PrepareErrorResponse(response, responseSize);
//		return;
//	}

	prvCONTROL_PrepareOkResponse(response, responseSize, "", 0);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "RGB Color sucessfully set\r\n");
}

/**
 * @brief	Enable charging
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingEnable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(CHARGER_SetChargingState(CHARGER_CHARGING_ENABLE, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging successfully enabled\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to enable charging\r\n");
		return;
	}
}

/**
 * @brief	Disable charging
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingDisable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(CHARGER_SetChargingState(CHARGER_CHARGING_DISABLE, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging successfully disabled\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to disable Charging\r\n");
		return;
	}
}
/**
 * @brief	Get charging status
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	charger_charging_state_t	chargingState = 0;
	char						chargingStateString[10];
	uint32_t					chargingStateStringLength = 0;

	//TODO check if DPCONTROL_STATUS_OK or CHARGER_STATUS_OK
	if(CHARGER_GetChargingState(&chargingState, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging status\r\n");
	}
	else
	{
		memset(chargingStateString, 0, sizeof(chargingStateString));
		chargingStateStringLength = sprintf(chargingStateString, "%u", (int)chargingState);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingStateString, chargingStateStringLength);
	}
}

/**
 * @brief	Set charging current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingCurrentSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					current;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &current);

	if(CHARGER_SetChargingCurrent(current, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging current %d [mA] set\r\n", current);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set charging current\r\n");
		return;
	}
}
/**
 * @brief	Get charging current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingCurrentGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint16_t					chargingCurrent = 0;
	char						chargingCurrentString[10];
	uint32_t					chargingCurrentStringLength = 0;

	if(CHARGER_GetChargingCurrent(&chargingCurrent, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging current\r\n");
	}
	else
	{
		memset(chargingCurrentString, 0, 10);
		chargingCurrentStringLength = sprintf(chargingCurrentString, "%u", (int)chargingCurrent);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingCurrentString, chargingCurrentStringLength);
	}
}
/**
 * @brief	Set charging termination current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingTermCurrentSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					current;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &current);

	if(CHARGER_SetChargingTermCurrent(current, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging termination current set to %d [%]\r\n", current);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set charging termination current\r\n");
		return;
	}
}
/**
 * @brief	Get charging termination current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingTermCurrentGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint16_t					chargingTermCurrent = 0;
	char						chargingTermCurrentString[10];
	uint32_t					chargingTermCurrentStringLength = 0;

	if(CHARGER_GetChargingTermCurrent(&chargingTermCurrent, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging status\r\n");
	}
	else
	{
		memset(chargingTermCurrentString, 0, sizeof(chargingTermCurrentString));
		chargingTermCurrentStringLength = sprintf(chargingTermCurrentString, "%u", (int)chargingTermCurrent);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingTermCurrentString, chargingTermCurrentStringLength);
	}
}
/**
 * @brief	Set charger termination voltage content
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingTermVoltageSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	float						voltage;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%f", &voltage);

	if(CHARGER_SetChargingTermVoltage(voltage, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging termination voltage set to %.2f [V]\r\n", voltage);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set charging termination voltage\r\n");
		return;
	}
}
/**
 * @brief	Get charger termination voltage content
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingTermVoltageGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	float						chargingTermVoltage = 0;
	char						chargingTermVoltageString[10];
	uint32_t					chargingTermVoltageStringLength = 0;

	if(CHARGER_GetChargingTermVoltage(&chargingTermVoltage, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging termination voltage\r\n");
	}
	else
	{
		memset(chargingTermVoltageString, 0, sizeof(chargingTermVoltageString));
		chargingTermVoltageStringLength = sprintf(chargingTermVoltageString, "%.2f", chargingTermVoltage);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingTermVoltageString, chargingTermVoltageStringLength);
	}
}

/**
 * @brief	Get charger register content
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargerReadReg(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	unsigned int				regAddr;
	uint8_t						regVal;
	char						responseContent[50];
	uint32_t					responseContentSize;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "reg", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%x", &regAddr);

	if(CHARGER_GetRegContent(regAddr, &regVal, 1000) == CHARGER_STATUS_OK)
	{
		responseContentSize = 0;
		memset(responseContent, 50, 0);
		responseContentSize = sprintf(responseContent, "OK: 0x%x", regVal);
		prvCONTROL_PrepareOkResponse(response, responseSize, responseContent, responseContentSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Reg %d successfully read\r\n", regAddr);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to read register current\r\n");
		return;
	}
}


////TODO: This function is introduced for testing purposes only. Remove it in production phase!
///**
// * @brief	Send message over status link
// * @param	arguments: arguments defined within control message
// * @param	argumentsLength: arguments message length
// * @param	response: response message content
// * @param	responseSize: length of response message
// * @retval	void
// */
//static void prvCONTROL_StatusLinkSendMessage(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
//{
//	cmparse_value_t	value;
//
//	memset(&value, 0, sizeof(cmparse_value_t));
//	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
//	{
//		prvCONTROL_PrepareErrorResponse(response, responseSize);
//		return;
//	}
//
//	if(CONTROL_StatusLinkSendMessage(value.value, CONTROL_STATUS_MESSAGE_TYPE_INFO, 2000) != CONTROL_STATUS_OK)
//	{
//		prvCONTROL_PrepareErrorResponse(response, responseSize);
//		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to send stream message\r\n");
//		return;
//	}
//	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
//}

/**
 * @brief	USB CDC RX callback
 * @param	data: to be copied
 * 			len: length of data
 * @retval	void
 */
static void prvCONTROL_USBRxCallback(uint8_t* data, uint32_t len)
{
	control_rx_packet_t pkt;
	BaseType_t higherPriorityTaskWoken = pdFALSE;

	if(len == 0 || len > CONTROL_BUFFER_SIZE){
		return;
	}

	memset(&pkt, 0, sizeof(control_rx_packet_t));
	memcpy(pkt.data, data, len);
	pkt.length = len;
	pkt.source = CONTROL_SOURCE_USB;

	xQueueSendFromISR(prvCONTROL_DATA.rxQueue, &pkt, &higherPriorityTaskWoken);
	portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/**
 * @brief	UART4 receive complete callback entry point.
 *
 * 			Call this from HAL_UART_RxCpltCallback when huart->Instance == UART4.
 *
 * 			Strategy: HAL is configured for 1-byte interrupt receive.  This
 * 			function accumulates bytes into a line buffer.  When CR or LF is
 * 			received (or the buffer is full) the assembled frame is posted to
 * 			the shared rxQueue and the accumulator is reset.  The ISR is then
 * 			re-armed for the next byte.
 *
 * @retval	void
 */
static void prvCONTROL_UART4_RxCallback(uint8_t byte)
{
	control_rx_packet_t pkt;
	BaseType_t higherPriorityTaskWoken = pdFALSE;

	if(byte == '\r' || byte == '\n')
	{
		/* End of line — only dispatch if we accumulated something */
		if(prvCONTROL_UART4_LineLen > 0)
		{
			memset(&pkt, 0, sizeof(control_rx_packet_t));
			memcpy(pkt.data, prvCONTROL_UART4_LineBuf, prvCONTROL_UART4_LineLen);
			pkt.length = prvCONTROL_UART4_LineLen;
			pkt.source = CONTROL_SOURCE_UART;

			xQueueSendFromISR(prvCONTROL_DATA.rxQueue, &pkt, &higherPriorityTaskWoken);

			/* Reset accumulator */
			memset(prvCONTROL_UART4_LineBuf, 0, CONTROL_BUFFER_SIZE);
			prvCONTROL_UART4_LineLen = 0;

			portYIELD_FROM_ISR(higherPriorityTaskWoken);
		}
	}
	else
	{
		/* Accumulate byte; discard silently if buffer would overflow */
		if(prvCONTROL_UART4_LineLen < CONTROL_BUFFER_SIZE)
		{
			prvCONTROL_UART4_LineBuf[prvCONTROL_UART4_LineLen] = byte;
			prvCONTROL_UART4_LineLen += 1;
		}
		else
		{
			/* Buffer overflow — discard and reset */
			memset(prvCONTROL_UART4_LineBuf, 0, CONTROL_BUFFER_SIZE);
			prvCONTROL_UART4_LineLen = 0;
		}
	}
}

static control_status_t	prvCONTROL_InitUART()
{
	drv_uart_config_t channelConfig;

	channelConfig.baudRate = 115200;
	channelConfig.parityEnable = DRV_UART_PARITY_NONE;
	channelConfig.stopBitNo	= DRV_UART_STOPBIT_1;

	if(DRV_UART_Instance_Init(DRV_UART_INSTANCE_4, &channelConfig) != DRV_UART_STATUS_OK) return CONTROL_STATUS_ERROR;
	return CONTROL_STATUS_OK;
}

drv_usb_cdc_status_t	DRV_USB_CDC_RegisterRxCallback(drv_usb_cdc_rx_isr_callback rxcb);

/**
 * @brief	Main control service task
 * @param	pvParameter: value forwarded during task creation
 * @retval	void
 */
static void prvCONTROL_TaskFunc(void* pvParameter)
{
	control_rx_packet_t pkt;

	DRV_USB_CDC_RegisterRxCallback(prvCONTROL_USBRxCallback);
	DRV_UART_Instance_RegisterRxCallback(DRV_UART_INSTANCE_4, prvCONTROL_UART4_RxCallback);

	prvCONTROL_DATA.state = CONTROL_STATE_SERVICE;
	xSemaphoreGive(prvCONTROL_DATA.initSig);

	for(;;)
	{
		if(xQueueReceive(prvCONTROL_DATA.rxQueue, &pkt, portMAX_DELAY) != pdTRUE)
		{
			continue;
		}

		memset(prvCONTROL_DATA.requestBuffer, 0, CONTROL_BUFFER_SIZE);
		memset(prvCONTROL_DATA.responseBuffer, 0, CONTROL_BUFFER_SIZE);
		prvCONTROL_DATA.responseBufferSize = 0;

		memcpy(prvCONTROL_DATA.requestBuffer, pkt.data, pkt.length);

		CMPARSE_Execute(prvCONTROL_DATA.requestBuffer, prvCONTROL_DATA.responseBuffer, &prvCONTROL_DATA.responseBufferSize);

		if(prvCONTROL_DATA.responseBufferSize != 0)
		{
			/* Route response back through the originating transport */
			switch(pkt.source)
			{
				case CONTROL_SOURCE_USB:
					if(DRV_USB_CDC_IsConnected())
					{
						DRV_USB_CDC_TransferData(
							(uint8_t*)prvCONTROL_DATA.responseBuffer,
							prvCONTROL_DATA.responseBufferSize,
							1000);
					}
					break;

				case CONTROL_SOURCE_UART:
					DRV_UART_TransferData(DRV_UART_INSTANCE_4, (uint8_t*)prvCONTROL_DATA.responseBuffer, prvCONTROL_DATA.responseBufferSize, 1000);
					break;

				default:
					break;
			}
		}
	}
}
/**
 * @brief	Status link task
 * @param	pvParameter: pointer to link status instance
 * @retval	void
 */
static void prvCONTROL_StatusLinkTaskFunc(void* pvParameter)
{
	control_status_link_data_t* linkData = (control_status_link_data_t*)pvParameter;
	control_status_message_t	msg;

	xSemaphoreGive(linkData->initSig);
	linkData->state = CONTROL_STATE_SERVICE;

	for(;;)
	{
		if(xQueueReceive(linkData->messageQueue, &msg, portMAX_DELAY) == pdTRUE)
		{
			if(DRV_USB_CDC_IsConnected())
			{
				linkData->linkState = CONTROL_LINK_STATE_UP;
				DRV_USB_CDC_TransferData((uint8_t*)msg.message, (uint16_t)msg.messageSize, 1000);
			}
			else
			{
				linkData->linkState = CONTROL_LINK_STATE_DOWN;
			}
		}
	}
}

control_status_t 	CONTROL_Init(uint32_t initTimeout){

	memset(&prvCONTROL_DATA, 0, sizeof(control_data_t));
	prvCONTROL_DATA.state = CONTROL_STATE_INIT;

	/* Init UART4 line accumulator */
	memset(prvCONTROL_UART4_LineBuf, 0, CONTROL_BUFFER_SIZE);
	prvCONTROL_UART4_LineLen  = 0;

	if(prvCONTROL_InitUART() != CONTROL_STATUS_OK)
	{
		prvCONTROL_DATA.state	= CONTROL_STATUS_ERROR;
		return CONTROL_STATUS_ERROR;
	}

	prvCONTROL_DATA.initSig = xSemaphoreCreateBinary();
	if(prvCONTROL_DATA.initSig == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_DATA.guard = xSemaphoreCreateMutex();
	if(prvCONTROL_DATA.guard == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_DATA.rxQueue = xQueueCreate(4, sizeof(control_rx_packet_t));
	if(prvCONTROL_DATA.rxQueue == NULL) return CONTROL_STATUS_ERROR;

	if(xTaskCreate(
				prvCONTROL_TaskFunc,
				CONTROL_TASK_NAME,
				CONTROL_TASK_STACK,
				NULL,
				CONTROL_TASK_PRIO, &prvCONTROL_DATA.taskHandle) != pdPASS) return CONTROL_STATUS_ERROR;


	/* Add commands */
	CMPARSE_AddCommand("", 									prvCONTROL_UndefinedCommand);
	CMPARSE_AddCommand("device hello", 						prvCONTROL_GetDeviceName);
	CMPARSE_AddCommand("device setname", 					prvCONTROL_SetDeviceName);

	CMPARSE_AddCommand("device rgb setcolor",     			prvCONTROL_SetRGBColor);

	CMPARSE_AddCommand("charger charging enable",       	prvCONTROL_ChargingEnable);
	CMPARSE_AddCommand("charger charging disable",      	prvCONTROL_ChargingDisable);
	CMPARSE_AddCommand("charger charging get",      		prvCONTROL_ChargingGet);

	CMPARSE_AddCommand("charger charging current set",  	prvCONTROL_ChargingCurrentSet);
	CMPARSE_AddCommand("charger charging current get",  	prvCONTROL_ChargingCurrentGet);
	CMPARSE_AddCommand("charger charging termcurrent set",  prvCONTROL_ChargingTermCurrentSet);
	CMPARSE_AddCommand("charger charging termcurrent get",  prvCONTROL_ChargingTermCurrentGet);
	CMPARSE_AddCommand("charger charging termvoltage set",  prvCONTROL_ChargingTermVoltageSet);
	CMPARSE_AddCommand("charger charging termvoltage get",  prvCONTROL_ChargingTermVoltageGet);
	CMPARSE_AddCommand("charger reg read",  				prvCONTROL_ChargerReadReg);


	if(xSemaphoreTake(prvCONTROL_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CONTROL_STATUS_ERROR;

	memset(&prvCONTROL_STATUS_LINK_DATA[0], 0, sizeof(control_status_link_data_t));
	prvCONTROL_STATUS_LINK_DATA[0].state	= CONTROL_STATE_INIT;
	prvCONTROL_STATUS_LINK_DATA[0].linkState= CONTROL_LINK_STATE_DOWN;

	prvCONTROL_STATUS_LINK_DATA[0].initSig = xSemaphoreCreateBinary();
	if(prvCONTROL_STATUS_LINK_DATA[0].initSig == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_STATUS_LINK_DATA[0].guard = xSemaphoreCreateMutex();
	if(prvCONTROL_STATUS_LINK_DATA[0].guard == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_STATUS_LINK_DATA[0].messageQueue = xQueueCreate(CONF_CONTROL_STATUS_LINK_MESSAGES_MAX_NO, sizeof(control_status_message_t));
	if(prvCONTROL_STATUS_LINK_DATA[0].messageQueue == NULL) return CONTROL_STATUS_ERROR;

	if(xTaskCreate(
					prvCONTROL_StatusLinkTaskFunc,
					CONTROL_STATUS_LINK_TASK_NAME,
					CONTROL_STATUS_LINK_TASK_STACK,
					(void*)&prvCONTROL_STATUS_LINK_DATA[0],
					CONTROL_TASK_PRIO, &prvCONTROL_STATUS_LINK_DATA[0].taskHandle) != pdPASS) return CONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvCONTROL_STATUS_LINK_DATA[0].initSig, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CONTROL_STATUS_ERROR;

	return CONTROL_STATUS_OK;
}


control_status_t 	CONTROL_StatusLinkSendMessage(const char* message, contol_status_message_type_t type, uint32_t timeout)
{
	if(prvCONTROL_STATUS_LINK_DATA[0].linkState != CONTROL_LINK_STATE_UP) return CONTROL_STATUS_ERROR;
	uint32_t messageSize = strlen(message);
	control_status_message_t messageData;
	if(messageSize > CONTROL_BUFFER_SIZE) return CONTROL_STATUS_ERROR;
	memcpy(messageData.message, message, messageSize);
	messageData.messageSize = messageSize;
	messageData.type = type;
	if(xQueueSend(prvCONTROL_STATUS_LINK_DATA[0].messageQueue,&messageData,timeout) != pdPASS) return CONTROL_STATUS_ERROR;
	return CONTROL_STATUS_OK;
}

control_status_t 	CONTROL_StatusLinkSendMessageFromISR(const char* message, contol_status_message_type_t type, uint32_t timeout)
{
	if(prvCONTROL_STATUS_LINK_DATA[0].linkState != CONTROL_LINK_STATE_UP) return CONTROL_STATUS_ERROR;
	uint32_t messageSize = strlen(message);
	control_status_message_t messageData;
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
	if(messageSize > CONTROL_BUFFER_SIZE) return CONTROL_STATUS_ERROR;
	memcpy(messageData.message, message, messageSize);
	messageData.messageSize = messageSize;
	messageData.type = type;
	if(xQueueSendFromISR(prvCONTROL_STATUS_LINK_DATA[0].messageQueue,&messageData, &pxHigherPriorityTaskWoken) != pdPASS) return CONTROL_STATUS_ERROR;
	portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	return CONTROL_STATUS_OK;
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
