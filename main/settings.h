#ifndef SETTINGS_H
#define SETTINGS_H

#include "common.h"
#include "mqtt.h"

/**
 * Initialization variables
 */
extern int device_ready;

/**
 * General variable settings
 */
#define DEVICE_SERIAL_LENGTH    32
#define DEVICE_ID_LENGTH        12
#define MQTT_SERVER_LENGTH       128
#define MQTT_PROTOCOL_LENGTH     10
#define MQTT_USER_LENGTH         64
#define MQTT_PASSWORD_LENGTH     64
#define MQTT_PREFIX_LENGTH       128
#define HA_PREFIX_LENGTH         128

#define SENSOR_OFFSET_MIN    -5.0
#define SENSOR_OFFSET_MAX    5.0

#define SENSOR_READ_INTERVAL_MIN    1000
#define SENSOR_READ_INTERVAL_MAX    60000

#define SENSOR_SAMPLING_COUNT_MIN    1
#define SENSOR_SAMPLING_COUNT_MAX    100

#define SENSOR_SAMPLING_INTERVAL_MIN    1
#define SENSOR_SAMPLING_INTERVAL_MAX    100

#define SENSOR_SAMPLING_MEDIAN_DEVIATION_MIN    1
#define SENSOR_SAMPLING_MEDIAN_DEVIATION_MAX    100


#define SENSOR_LINEAR_MULTIPLIER_MIN    1
#define SENSOR_LINEAR_MULTIPLIER_MAX    1000000

#define HA_UPDATE_INTERVAL_MIN  60000           // Once a minute
#define HA_UPDATE_INTERVAL_MAX  86400000        // Once a day (24 hr)

/**
 * General constants
 */
#define S_NAMESPACE         "settings"

#define S_DEVICE_FAMILY       "sensor"

/**
 * Settings keys
 * 
 * NOTE: MAX key size is 15 characters
 * 
 * DO NOT CHANGE, unless you definitelly know why you need this
 */


#define S_KEY_DEVICE_ID         "device_id"
#define S_KEY_DEVICE_SERIAL     "device_serial"

#define S_KEY_MQTT_CONNECT      "mqtt_connect"       
#define S_KEY_MQTT_SERVER       "mqtt_server"
#define S_KEY_MQTT_PORT         "mqtt_port"
#define S_KEY_MQTT_PROTOCOL     "mqtt_protocol"
#define S_KEY_MQTT_USER         "mqtt_user"
#define S_KEY_MQTT_PASSWORD     "mqtt_password"
#define S_KEY_MQTT_PREFIX       "mqtt_prefix"

#define S_KEY_HA_PREFIX                 "ha_prefix"
#define S_KEY_HA_UPDATE_INTERVAL        "ha_upd_intervl"

#define S_KEY_SENSOR_OFFSET             "sensor_offset"
#define S_KEY_SENSOR_LINEAR_MULTIPLIER  "sensor_multipl"
#define S_KEY_SENSOR_READ_INTERVAL      "sensor_intervl"

#define S_KEY_SENSOR_SAMPLING_COUNT                "sensor_samples"
#define S_KEY_SENSOR_SAMPLING_INTERVAL             "sensor_smp_int"
#define S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION     "sensor_deviate"


/**
 * Settings default values
 */
#define S_DEFAULT_DEVICE_ID         ""
#define S_DEFAULT_DEVICE_SERIAL     ""

#define S_DEFAULT_MQTT_CONNECT      MQTT_SENSOR_MODE_DISABLE
#define S_DEFAULT_MQTT_SERVER       "127.0.0.1"
#define S_DEFAULT_MQTT_PORT         1883
#define S_DEFAULT_MQTT_PROTOCOL     "mqtt"
#define S_DEFAULT_MQTT_USER         ""
#define S_DEFAULT_MQTT_PASSWORD     ""
#define S_DEFAULT_MQTT_PREFIX       "pressure_sensor"

#define S_DEFAULT_HA_PREFIX             "homeassistant"
#define S_DEFAULT_HA_UPDATE_INTERVAL    600000              // Update Home Assistant definitions every 10 minutes

#define S_DEFAULT_SENSOR_OFFSET               0.471
#define S_DEFAULT_SENSOR_LINEAR_MULTIPLIER    250000
#define S_DEFAULT_SENSOR_READ_INTERVAL        3000      // ms

#define S_DEFAULT_SENSOR_SAMPLING_COUNT                 50  // Number of samples to collect per measurement
#define S_DEFAULT_SENSOR_SAMPLING_INTERVAL              10  // Interval between samples in milliseconds
#define S_DEFAULT_SENSOR_SAMPLING_MEDIAN_DEVIATION      10  // Threshold percentage for filtering


/**
 * Routines
 */ 

/**
 * @brief Initialize settings:
 *  - fill-in missing settings with default values
 * 
 */
esp_err_t settings_init();

/**
 * @brief Generate serial number for the device
 * 
 */
void generate_serial_number(char *serial_number);

#endif