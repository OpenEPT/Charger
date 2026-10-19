/**
 ******************************************************************************
 * @file    sstream.h
 *
 * @brief   Samples Stream service. Samples the battery voltage (PC3 / ADC1_IN4)
 *          every 0.1 s via the AnalogIN driver and streams each value to the PC
 *          over USB CDC, but only while streaming has been explicitly enabled
 *          by a command from the host. Modeled on the EPP SSTREAM service, but
 *          transported over USB instead of the LAN.
 ******************************************************************************
 */
#ifndef CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_
#define CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_

#include <stdint.h>

typedef enum
{
    SSTREAM_STATUS_OK = 0,
    SSTREAM_STATUS_ERROR
} sstream_status_t;

/**
 * @brief  Initialize the samples-stream service (queue + task + driver callback).
 * @retval ::sstream_status_t
 */
sstream_status_t SSTREAM_Init(void);

/**
 * @brief  Start sampling and streaming battery voltage over USB.
 */
sstream_status_t SSTREAM_StartStream(uint32_t timeout);

/**
 * @brief  Stop sampling/streaming battery voltage.
 */
sstream_status_t SSTREAM_StopStream(uint32_t timeout);

/**
 * @brief  Get the most recent battery-voltage ADC value (12-bit).
 */
sstream_status_t SSTREAM_GetLastValue(uint16_t* value);

#endif /* CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_ */
