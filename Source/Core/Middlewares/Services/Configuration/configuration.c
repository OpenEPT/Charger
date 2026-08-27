/**
 ******************************************************************************
 * @file    configuration.c
 *
 * @brief   Configuration service implementation.
 *          Contains central FreeRTOS task with basic state machine.
 *
 * @author  Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "configuration.h"
#include "configurationDef.h"
#include "logging.h"
#include "system.h"
#include "../../../HAL/AT24CS01/at24cs01.h"

/**
 * @defgroup CONFIGURATION_PRIVATE_STRUCTURES Configuration private structures
 * @{
 */
#define CONFIGURATION_MASK_UPDATE_FROM_FS   		0x00000001
#define CONFIGURATION_MASK_SET_PARAM        		0x00000002
#define CONFIGURATION_MASK_SAVE_TO_FS   			0x00000004
#define CONFIGURATION_MASK_CHARGER_TEST_BD			0x00000008	/*!< Request charger board EEPROM presence test */
#define CONFIGURATION_MASK_CHARGER_UPDATE_FROM_BD	0x00000010	/*!< Request charger configuration update from EEPROM */
/**
 * @brief Configuration internal data structure
 */
typedef struct
{
    configuration_state_t state;
    SemaphoreHandle_t     initSig;
    SemaphoreHandle_t     guard;
    TaskHandle_t          taskHandle;
    configuration_param_t params[CONFIGURATION_MAX_PARAMS];
    uint32_t              paramsCount;

    configuration_param_t lastParam;

} configuration_data_t;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PRIVATE_DATA Configuration private data
 * @{
 */

static configuration_data_t prvCONFIGURATION_DATA;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PRIVATE_FUNCTIONS Configuration private functions
 * @{
 */

/**
 * @brief Get parameter by key from runtime table
 *
 * @param key Parameter name
 * @return Pointer to parameter or NULL if not found
 */
static configuration_param_t* prvCONFIGURATION_GetParam(const char* key)
{
    if(key == NULL)
        return NULL;

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(strcmp(param->name, key) == 0)
        {
            return param;
        }
    }

    return NULL;
}

static void prvCONFIGURATION_SerializeToString(char* buffer,
                                               uint32_t maxSize,
                                               uint32_t* outSize)
{
    uint32_t offset = 0;

    if(buffer == NULL || outSize == NULL || maxSize == 0)
        return;

    buffer[0] = '\0';
    *outSize = 0;

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(param == NULL || param->name == NULL || param->value == NULL)
            continue;

        /* SKIP SYSTEM PARAMETERS */
        if(param->systemParam == 1U)
            continue;

        /* Safe length for value (prevents runaway strings) */
        size_t valueLen = strnlen((char*)param->value, CONFIGURATION_MAX_PARAM_VALUESIZE);

        /* Write safely using bounded value length */
        int written = snprintf(&buffer[offset],
                               maxSize - offset,
                               "%s:%.*s\r\n",
                               param->name,
                               (int)valueLen,
                               param->value);

        /* Check for error or overflow */
        if(written <= 0 || (offset + (uint32_t)written) >= maxSize)
        {
            /* Ensure string termination */
            buffer[maxSize - 1] = '\0';
            break;
        }

        offset += (uint32_t)written;
    }

    *outSize = offset;
}

static void prvCONFIGURATION_InitParams(void)
{
    configuration_param_t* defaults = CONFIGURATIONDEF_GetDefaults();
    uint32_t count = CONFIGURATIONDEF_GetDefaultsCount();

    prvCONFIGURATION_DATA.paramsCount = count;

    for(uint32_t i = 0; i < count; i++)
    {
        memcpy(&prvCONFIGURATION_DATA.params[i],
               &defaults[i],
               sizeof(configuration_param_t));
    }
}
static void prvCONFIGURATION_ReportDefaultParams(void)
{
    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(param == NULL || param->name == NULL || param->value == NULL)
            continue;

        if(param->defaultValue == 1U)
        {
            LOGGING_Write("CONFIG",
                          LOGGING_MSG_TYPE_WARNING,
                          "Default param used: %s = %s\r\n",
                          param->name,
                          param->value);
        }
    }
}



static void prvCONFIGURATION_UpdateSystemParamFromBD(void)
{
    uint8_t header[CONF_CONFIGURATION_HEADER_SIZE];
    uint32_t payloadSize = 0;

    char* line;
    char* sep;
    char* key;
    char* value;
    char* saveptr;

    static char payloadBuffer[AT24CS01_MEMORY_SIZE_BYTES];

    /* ===== READ HEADER ===== */
    if(AT24CS01_Read(0x0000, header, CONF_CONFIGURATION_HEADER_SIZE, 1000) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR,
                      "System BD read failed (header)\r\n");
        return;
    }

    uint32_t magic = 0;
    memcpy(&magic, &header[0], sizeof(uint32_t));

    if(magic != 0xA5A6A7A8)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR,
                      "Invalid system MAGIC\r\n");
        return;
    }

    memcpy(&payloadSize, &header[4], sizeof(uint32_t));

    if(payloadSize == 0 || payloadSize >= AT24CS01_MEMORY_SIZE_BYTES)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR,
                      "Invalid system payload size\r\n");
        return;
    }

    /* ===== READ PAYLOAD ===== */
    if(AT24CS01_Read(CONF_CONFIGURATION_HEADER_SIZE,
                   (uint8_t*)payloadBuffer,
                   payloadSize,
                   1000) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR,
                      "System BD read failed (payload)\r\n");
        return;
    }

    payloadBuffer[payloadSize] = '\0';

    /* ===== PARSE ===== */
    line = strtok_r(payloadBuffer, "\r\n", &saveptr);

    while(line != NULL)
    {
        sep = strchr(line, ':');

        if(sep != NULL)
        {
            *sep = '\0';
            key = line;
            value = sep + 1;

            /* trim key */
            while(*key == ' ' || *key == '\t') key++;

            /* trim value (left) */
            while(*value == ' ' || *value == '\t') value++;

            /* trim value (right) */
            char* end = value + strlen(value) - 1;
            while(end > value && (*end == ' ' || *end == '\t'))
            {
                *end = '\0';
                end--;
            }

            /* ===== SEARCH ONLY SYSTEM PARAMS ===== */
            for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
            {
                configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

                if(param->name == NULL || param->value == NULL)
                    continue;

                /* ONLY system params */
                if(param->systemParam == 0U)
                    continue;

                if(strcmp(param->name, key) == 0)
                {
                    size_t len = strnlen(value, CONFIGURATION_MAX_PARAM_VALUESIZE);

                    memcpy(param->value, value, len);
                    param->value[len] = '\0';

                    param->defaultValue = 0;

                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO,
                                  "System param: %s = %s\r\n",
                                  param->name,
                                  param->value);

                    break;
                }
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO,
                  "System configuration loaded from BD\r\n");
}



/**
 * @brief Configuration service main task
 * @param pvParameters: FreeRTOS task parameters
 * @retval void
 */
static void prvCONFIGURATION_Task(void *pvParameters)
{
    for(;;)
    {
        switch(prvCONFIGURATION_DATA.state)
        {
        case CONFIGURATION_STATE_INIT:

            /* Initialize EEPROM driver. */
            AT24CS01_Init();

            /* Check communication with EEPROM. */
            AT24CS01_Ping(1000);

            /* Initialize configuration parameters. */
            prvCONFIGURATION_InitParams();

            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration initialized\r\n");

            /* Update system parameters from EEPROM. */
            prvCONFIGURATION_UpdateSystemParamFromBD();

            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration updated from System memory\r\n");

            /* Report parameters using default values. */
            prvCONFIGURATION_ReportDefaultParams();

            /* Switch to service state. */
            prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_SERVICE;

            /* Signal that initialization is complete. */
            xSemaphoreGive(prvCONFIGURATION_DATA.initSig);

            break;

        case CONFIGURATION_STATE_SERVICE:

            int32_t value;

            /* Wait for a configuration request. */
            xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);

            /* Process a parameter update request. */
            if(value & CONFIGURATION_MASK_SET_PARAM)
            {
                if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
                    break;
                }

                configuration_param_t* param = NULL;

                /* Find the requested parameter. */
                for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
                {
                    if(strcmp(prvCONFIGURATION_DATA.params[i].name, prvCONFIGURATION_DATA.lastParam.name) == 0)
                    {
                        param = &prvCONFIGURATION_DATA.params[i];
                        break;
                    }
                }

                /* Update the parameter if it is writable. */
                if(param != NULL && param->readOnly == 0)
                {
                    strncpy(param->value, prvCONFIGURATION_DATA.lastParam.value, sizeof(param->value) - 1);
                    param->value[sizeof(param->value) - 1] = '\0';
                    param->defaultValue = 0;
                }

                xSemaphoreGive(prvCONFIGURATION_DATA.guard);
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to update system parameters from EEPROM. */
            if(value & CONFIGURATION_MASK_CHARGER_UPDATE_FROM_BD)
            {
                prvCONFIGURATION_UpdateSystemParamFromBD();
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            break;

        case CONFIGURATION_STATE_ERROR:

            /* Report configuration service error. */
            SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);

            /* Suspend the configuration task. */
            vTaskDelay(portMAX_DELAY);

            break;

        default:
            break;
        }
    }
}

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_FUNCTIONS Configuration public functions
 * @{
 */

configuration_status_t CONFIGURATION_Init(uint32_t initTimeout)
{
    memset(&prvCONFIGURATION_DATA, 0, sizeof(configuration_data_t));

    prvCONFIGURATION_DATA.initSig = xSemaphoreCreateBinary();
    if(prvCONFIGURATION_DATA.initSig == NULL)
        return CONFIGURATION_STATUS_ERROR;

    prvCONFIGURATION_DATA.guard = xSemaphoreCreateMutex();
    if(prvCONFIGURATION_DATA.guard == NULL)
        return CONFIGURATION_STATUS_ERROR;

    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_INIT;

    if(xTaskCreate(
            prvCONFIGURATION_Task,
            CONFIGURATION_TASK_NAME,
            CONFIGURATION_TASK_STACK,
            NULL,
            CONFIGURATION_TASK_PRIO,
            &prvCONFIGURATION_DATA.taskHandle) != pdPASS)
    {
        return CONFIGURATION_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig,
            pdMS_TO_TICKS(initTimeout)) != pdPASS)
    {
        return CONFIGURATION_STATUS_ERROR;
    }

    return CONFIGURATION_STATUS_OK;
}
configuration_status_t CONFIGURATION_UpdateFromFS(uint32_t timeout)
{
    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_UPDATE_FROM_FS, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;
    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag)
{
    if(key == NULL || parameter == NULL || paramSize == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_GetParam(key);

    if(param != NULL)
    {
        uint16_t len = strlen(param->value);
        memcpy(parameter, param->value, len);
        parameter[len] = '\0';
        *paramSize = len;
        *defaultFlag = param->defaultValue;
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_OK;
    }

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);
    return CONFIGURATION_STATUS_ERROR;
}
configuration_status_t CONFIGURATION_GetParameter_Int(const char* key, int32_t* value, uint8_t* defaultFlag)
{
    if(key == NULL || value == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_INT)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    *value = (int32_t)strtol((char*)param->value, NULL, 10);
    *defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);
    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_GetParameter_Float(const char* key, float* value, uint8_t* defaultFlag)
{
    if(key == NULL || value == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_FLOAT)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    float tmp;
	int ret = sscanf((char*)param->value, "%f", &tmp);

	if(ret != 1)
	{
		xSemaphoreGive(prvCONFIGURATION_DATA.guard);
		return CONFIGURATION_STATUS_ERROR;
	}

	*value = tmp;
	*defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);
    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_GetParameter_String(const char* key, char* buffer, uint16_t bufferSize, uint8_t* defaultFlag)
{
    uint16_t size;

    if(key == NULL || buffer == NULL || defaultFlag == NULL || bufferSize == 0) return CONFIGURATION_STATUS_ERROR;

    if(CONFIGURATION_GetParameter(key, buffer, &size, defaultFlag) != CONFIGURATION_STATUS_OK) return CONFIGURATION_STATUS_ERROR;

    if(size >= bufferSize) return CONFIGURATION_STATUS_ERROR;

    buffer[size] = '\0';

    return CONFIGURATION_STATUS_OK;
}


configuration_status_t CONFIGURATION_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout)
{
    if(key == NULL || parameter == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    strncpy(prvCONFIGURATION_DATA.lastParam.name, key, sizeof(prvCONFIGURATION_DATA.lastParam.name) - 1);
    strncpy(prvCONFIGURATION_DATA.lastParam.value, parameter, sizeof(prvCONFIGURATION_DATA.lastParam.value) - 1);

    prvCONFIGURATION_DATA.lastParam.name[sizeof(prvCONFIGURATION_DATA.lastParam.name) - 1] = '\0';
    prvCONFIGURATION_DATA.lastParam.value[sizeof(prvCONFIGURATION_DATA.lastParam.value) - 1] = '\0';

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_SET_PARAM, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}
configuration_status_t CONFIGURATION_SetParameter_String(const char* key, const char* value, uint32_t timeout)
{
    if(key == NULL || value == NULL) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_UpdateParamValue(key, (char*)value, strlen(value), timeout);
}
configuration_status_t CONFIGURATION_SetParameter_Int(const char* key, int32_t value, uint32_t timeout)
{
    char buffer[32];

    if(key == NULL) return CONFIGURATION_STATUS_ERROR;

    int len = snprintf(buffer, sizeof(buffer), "%ld", value);
    if(len <= 0 || len >= sizeof(buffer)) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_UpdateParamValue(key, buffer, len, timeout);
}
configuration_status_t CONFIGURATION_SetParameter_Float(const char* key, float value, uint32_t timeout)
{
    char buffer[32];

    if(key == NULL) return CONFIGURATION_STATUS_ERROR;

    int len = snprintf(buffer, sizeof(buffer), "%.4f", value);
    if(len <= 0 || len >= sizeof(buffer)) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_UpdateParamValue(key, buffer, len, timeout);
}
configuration_status_t CONFIGURATION_StoreToFS(uint32_t timeout)
{
    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_SAVE_TO_FS, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;
    return CONFIGURATION_STATUS_OK;
}
configuration_status_t CONFIGURATION_UpdateFromBD(uint32_t timeout)
{

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_UPDATE_FROM_BD, eSetBits) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS)
        return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}



/**
 * @}
 */
