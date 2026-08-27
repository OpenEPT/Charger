#include "configuration.h"
#include "globalConfig.h"
#include "configurationDef.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


/**
 * @brief Default charger configuration parameter table
 *
 * Contains default values and properties of all configuration parameters
 * associated with the optional charger module.
 */
static configuration_param_t prvCONFIGURATION_DEFAULTS[] =
{
        {
            .name = "HW_SER",
            .value = "0123456789",
            .type = CONFIGURATION_PARAM_TYPE_STRING,
            .readOnly = 1,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "FW_VER",
            .value = "1.0.0",
            .type = CONFIGURATION_PARAM_TYPE_STRING,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "CH_CUR",
            .value = "100",
            .type = CONFIGURATION_PARAM_TYPE_INT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "TERM_VOLT",
            .value = "4.2",
            .type = CONFIGURATION_PARAM_TYPE_FLOAT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "TERM_CUR",
            .value = "3",
            .type = CONFIGURATION_PARAM_TYPE_INT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "MAX_CUR",
            .value = "5",
            .type = CONFIGURATION_PARAM_TYPE_INT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        }
};

#define CONFIGURATION_DEFAULTS_COUNT \
    (sizeof(prvCONFIGURATION_DEFAULTS) / sizeof(configuration_param_t))

configuration_param_t* CONFIGURATIONDEF_GetDefaults(void)
{
    return prvCONFIGURATION_DEFAULTS;
}

uint32_t CONFIGURATIONDEF_GetDefaultsCount(void)
{
    return CONFIGURATION_DEFAULTS_COUNT;
}
