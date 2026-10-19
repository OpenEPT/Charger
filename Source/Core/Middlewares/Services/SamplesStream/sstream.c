/**
 ******************************************************************************
 * @file    sstream.c
 * @brief   Samples Stream service implementation (battery voltage over USB).
 ******************************************************************************
 */
#include "sstream.h"

#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "drv_ain.h"
#include "drv_usb_cdc.h"
#include "logging.h"

#define SSTREAM_TASK_NAME       "SStream Task"
#define SSTREAM_TASK_STACK      256U
#define SSTREAM_TASK_PRIO       3U
#define SSTREAM_QUEUE_LENGTH    8U
#define SSTREAM_TX_TIMEOUT      100U

typedef struct
{
    TaskHandle_t      taskHandle;
    QueueHandle_t     valueQueue;     /*!< ADC values pushed from the EOC ISR */
    volatile uint8_t  streamingEnabled;
    volatile uint16_t lastValue;
} sstream_data_t;

static sstream_data_t prvSSTREAM_DATA;

/**
 * @brief Driver sample callback, invoked from the ADC EOC ISR. Forwards the
 *        latest value to the stream task through the queue.
 */
static void prvSSTREAM_SampleCallback(uint16_t value)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(prvSSTREAM_DATA.valueQueue, &value, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/**
 * @brief Stream task: waits for ADC values and sends them over USB while
 *        streaming is enabled. One value per 0.1 s.
 */
static void prvSSTREAM_TaskFunc(void* param)
{
    (void)param;
    uint16_t value;
    char     msg[32];

    /* Register the driver callback once the task is up. */
    DRV_AIN_RegisterSampleCallback(prvSSTREAM_SampleCallback);

    for(;;)
    {
        if(xQueueReceive(prvSSTREAM_DATA.valueQueue, &value, portMAX_DELAY) != pdTRUE)
            continue;

        prvSSTREAM_DATA.lastValue = value;

        if(prvSSTREAM_DATA.streamingEnabled && DRV_USB_CDC_IsConnected())
        {
            int len = snprintf(msg, sizeof(msg), "bat voltage %u\r\n", (unsigned int)value);
            if(len > 0)
                DRV_USB_CDC_TransferData((uint8_t*)msg, (uint16_t)len, SSTREAM_TX_TIMEOUT);
        }
    }
}

sstream_status_t SSTREAM_Init(void)
{
    memset(&prvSSTREAM_DATA, 0, sizeof(prvSSTREAM_DATA));

    prvSSTREAM_DATA.valueQueue = xQueueCreate(SSTREAM_QUEUE_LENGTH, sizeof(uint16_t));
    if(prvSSTREAM_DATA.valueQueue == NULL) return SSTREAM_STATUS_ERROR;

    if(xTaskCreate(prvSSTREAM_TaskFunc, SSTREAM_TASK_NAME, SSTREAM_TASK_STACK, NULL,
                   SSTREAM_TASK_PRIO, &prvSSTREAM_DATA.taskHandle) != pdPASS)
        return SSTREAM_STATUS_ERROR;

    return SSTREAM_STATUS_OK;
}

sstream_status_t SSTREAM_StartStream(uint32_t timeout)
{
    (void)timeout;
    prvSSTREAM_DATA.streamingEnabled = 1U;
    if(DRV_AIN_Start(DRV_AIN_ADC_1) != DRV_AIN_STATUS_OK) return SSTREAM_STATUS_ERROR;
    LOGGING_Write("SStream", LOGGING_MSG_TYPE_INFO, "Voltage streaming started\r\n");
    return SSTREAM_STATUS_OK;
}

sstream_status_t SSTREAM_StopStream(uint32_t timeout)
{
    (void)timeout;
    prvSSTREAM_DATA.streamingEnabled = 0U;
    DRV_AIN_Stop(DRV_AIN_ADC_1);
    LOGGING_Write("SStream", LOGGING_MSG_TYPE_INFO, "Voltage streaming stopped\r\n");
    return SSTREAM_STATUS_OK;
}

sstream_status_t SSTREAM_GetLastValue(uint16_t* value)
{
    if(value == NULL) return SSTREAM_STATUS_ERROR;
    *value = prvSSTREAM_DATA.lastValue;
    return SSTREAM_STATUS_OK;
}
