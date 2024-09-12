#ifndef _ZIGBEE_H
#define _ZIGBEE_H

#include "common.h"

#include "driver/gpio.h"
#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_ESP_SENSOR_ENDPOINT          10      /* esp temperature sensor device endpoint, used for temperature measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */

#define ESP_PRESSURE_SENSOR_UPDATE_INTERVAL (1)     /* Local sensor update interval (second) */
#define ESP_PRESSURE_SENSOR_MIN_VALUE       (-5000)   /* Local sensor min measured value (Pa) */
#define ESP_PRESSURE_SENSOR_MAX_VALUE       (32000)    /* Local sensor max measured value (Pa) */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME               "\x09""PAVLYUKCON"
#define MODEL_IDENTIFIER                "\x07""PS01"

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_ZR_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

#define GPIO_INPUT_IO_TOGGLE_SWITCH  GPIO_NUM_9

/**
 * @brief Zigbee HA standard temperature sensor clusters.
 *
 */
typedef struct esp_zb_pressure_sensor_cfg_s {
    esp_zb_basic_cluster_cfg_t basic_cfg;                /*!<  Basic cluster configuration, @ref esp_zb_basic_cluster_cfg_s */
    esp_zb_identify_cluster_cfg_t identify_cfg;          /*!<  Identify cluster configuration, @ref esp_zb_identify_cluster_cfg_s */
    esp_zb_pressure_meas_cluster_cfg_t pressure_meas_cfg; /*!<  Pressure measurement cluster configuration, @ref esp_zb_pressure_meas_cluster_cfg_t */
} esp_zb_pressure_sensor_cfg_t;

typedef enum {
    SWITCH_ON_CONTROL,
    SWITCH_OFF_CONTROL,
    SWITCH_ONOFF_TOGGLE_CONTROL,
    SWITCH_LEVEL_UP_CONTROL,
    SWITCH_LEVEL_DOWN_CONTROL,
    SWITCH_LEVEL_CYCLE_CONTROL,
    SWITCH_COLOR_CONTROL,
} switch_func_t;

typedef struct {
    uint32_t pin;
    switch_func_t func;
} switch_func_pair_t;

/**
 * @brief Zigbee HA standard pressure sensor device default config value.
 *
 */
#define ESP_ZB_DEFAULT_PRESSURE_SENSOR_CONFIG()                                                  \
    {                                                                                               \
        .basic_cfg =                                                                                \
            {                                                                                       \
                .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                          \
                .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                        \
            },                                                                                      \
        .identify_cfg =                                                                             \
            {                                                                                       \
                .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                   \
            },                                                                                      \
        .pressure_meas_cfg =                                                                            \
            {                                                                                       \
                .measured_value = ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_DEFAULT_VALUE,               \
                .min_value = ESP_ZB_ZCL_PATTR_RESSURE_MEASUREMENT_MIN_VALUE_DEFAULT_VALUE,                \
                .max_value = ESP_ZB_ZCL_PATTR_RESSURE_MEASUREMENT_MAX_VALUE_DEFAULT_VALUE,                \
            },                                                                                      \
    }

/* Functions */
esp_err_t zigbee_init(void);
static void esp_zb_task(void *pvParameters);

#endif // ZIGBEE_H