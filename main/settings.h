#ifndef SETTINGS_H
#define SETTINGS_H

#include "common.h"
#include "mqtt.h"
#include "cJSON.h"

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
#define CA_CERT_LENGTH           8192
#define NET_LOGGING_HOST_LENGTH   256

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

#define OTA_UPDATE_URL_LENGTH  256

#define MEMGUARD_THRESHOLD_MIN        16384     // 16k
#define MEMGUARD_THRESHOLD_MAX        131072    // 128k

#define MEMGRD_MODE_DISABLED    0
#define MEMGRD_MODE_WARN        1
#define MEMGRD_MODE_RESTART     2

#define OTA_UPDATE_RESET_CONFIG_MIN 0
#define OTA_UPDATE_RESET_CONFIG_MAX 1

/**
 * General constants
 */
#define S_NAMESPACE           "settings"
#define S_DEVICE_FAMILY       "sensor"
#define WIFI_NAMESPACE        "nvs.net80211"

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

#define S_KEY_OTA_UPDATE_URL            "ota_update_url"
#define S_KEY_OTA_UPDATE_RESET_CONFIG   "ota_upd_rescfg"

#define S_KEY_NET_LOGGING_TYPE     "net_log_type"
#define S_KEY_NET_LOGGING_HOST     "net_log_host"
#define S_KEY_NET_LOGGING_PORT     "net_log_port"
#define S_KEY_NET_LOGGING_KEEP_STDOUT "net_log_stdout"

#define S_KEY_STATUS_MEMGUARD_MODE          "memgrd_mode"
#define S_KEY_STATUS_MEMGUARD_THRESHOLD     "memgrd_trshld"


/**
 * Settings default values
 */
#define S_DEFAULT_DEVICE_ID         ""
#define S_DEFAULT_DEVICE_SERIAL     ""

#define S_DEFAULT_MQTT_CONNECT      MQTT_CONN_MODE_DISABLE
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

#define S_DEFAULT_OTA_UPDATE_URL \
    "https://dist-repo-public.s3.eu-central-1.amazonaws.com/firmware/ESPPressureSensor/" CONFIG_IDF_TARGET "/latest/ESPZPressureSensor.bin"
#define S_DEFAULT_OTA_UPDATE_RESET_CONFIG       0   // 0 - Do not reset config, 1 - Reset config to defaults (except Wi-Fi) before applying update. 
    // This can be useful if the device becomes inaccessible due to misconfiguration and you want to ensure that the new firmware will be applied with default settings.

#define S_DEFAULT_NET_LOGGING_TYPE          0       // 0 - Disabled, 1 - UDP, 2 - TCP, 3 - MQTT
#define S_DEFAULT_NET_LOGGING_HOST          "127.0.0.1" // will be used as host for UDP/TCP and as MQTT topic for MQTT logging
#define S_DEFAULT_NET_LOGGING_PORT          514
#define S_DEFAULT_NET_LOGGING_KEEP_STDOUT   1       // 0 - disable stdout logging, 1 - enable stdout logging

#define S_DEFAULT_STATUS_MEMGUARD_MODE          MEMGRD_MODE_DISABLED       // 0 - Disabled, 1 - Warn, 2 - Restart
#define S_DEFAULT_STATUS_MEMGUARD_THRESHOLD     65536    // 64k in bytes


/**
 * CA Paths
 */

#define CA_CERT_PATH_MQTTS  "/spiffs/ca-mqtts.crt"
#define CA_CERT_PATH_HTTPS  "/spiffs/ca-https.crt"

/**
 * OTA related settings
 */
static const char *OTA_TAG = "OTAUpdate";
#define OTA_STORAGE_IMAGE_NAME  "storage.bin"
#define DO_OTA_STORAGE_UPDATE       true

typedef struct {
    char ota_url[OTA_UPDATE_URL_LENGTH];  // Adjust the size if needed
} ota_update_param_t;

/**
 * Settings management data structures and types
 */
// The longest of all string settings, except CA certificates
#define OLD_VALUE_STR_MAX_LEN    256

typedef enum {
    SETTING_TYPE_UINT32,
    SETTING_TYPE_UINT16,
    SETTING_TYPE_STRING,
    SETTING_TYPE_BLOB,
    SETTING_TYPE_FLOAT,
    SETTING_TYPE_DOUBLE,
} settings_type_t;

typedef struct {
    char msg[256];
    esp_err_t err_code;
    bool has_old;
    char old_value_str[OLD_VALUE_STR_MAX_LEN];   // generic printable form
} setting_update_msg_t;

typedef esp_err_t (*setting_handler_t)( const char *key,
                                        const cJSON *value,                                    
                                        setting_update_msg_t *out);

typedef struct {
    const char *key;
    setting_handler_t handler;      // optional per-setting extra validation/side-effects
    size_t max_str_size;            // for STRING (and maybe BLOB base64 if you choose)
    settings_type_t type_t;
} setting_entry_t;

/**
 * @brief: Setting handlers implementation
 */
static esp_err_t handle_setting_ha_upd_intervl(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_mqtt_connect(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_mqtt_port(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_net_log_type(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_net_log_port(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_net_log_stdout(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_memgrd_mode(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_memgrd_trshld(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_ota_upd_rescfg(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_sensor_offset(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_sensor_linear_multiplier(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_sensor_read_interval(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_sensor_samples(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_sensor_smp_int(const char *key, const cJSON *v, setting_update_msg_t *out);
static esp_err_t handle_setting_sensor_smp_deviate(const char *key, const cJSON *v, setting_update_msg_t *out);

/**
 * @brief: Setting handlers mapping table
 */
static const setting_entry_t s_settings[] = {
    { S_KEY_OTA_UPDATE_URL, NULL, OTA_UPDATE_URL_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_HA_UPDATE_INTERVAL, handle_setting_ha_upd_intervl, 0, SETTING_TYPE_UINT32 },
    { S_KEY_MQTT_CONNECT, handle_setting_mqtt_connect, 0, SETTING_TYPE_UINT16 },
    { S_KEY_MQTT_SERVER, NULL, MQTT_SERVER_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_MQTT_PORT, handle_setting_mqtt_port, 0, SETTING_TYPE_UINT16 },
    { S_KEY_MQTT_PROTOCOL, NULL, MQTT_PROTOCOL_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_MQTT_USER, NULL, MQTT_USER_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_MQTT_PASSWORD, NULL, MQTT_PASSWORD_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_MQTT_PREFIX, NULL, MQTT_PREFIX_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_HA_PREFIX, NULL, HA_PREFIX_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_NET_LOGGING_TYPE, handle_setting_net_log_type, 0, SETTING_TYPE_UINT16 },
    { S_KEY_NET_LOGGING_HOST, NULL, NET_LOGGING_HOST_LENGTH, SETTING_TYPE_STRING },
    { S_KEY_NET_LOGGING_PORT, handle_setting_net_log_port, 0, SETTING_TYPE_UINT16 },
    { S_KEY_NET_LOGGING_KEEP_STDOUT, handle_setting_net_log_stdout, 0, SETTING_TYPE_UINT16 },
    { S_KEY_STATUS_MEMGUARD_MODE, handle_setting_memgrd_mode, 0, SETTING_TYPE_UINT16 },
    { S_KEY_STATUS_MEMGUARD_THRESHOLD, handle_setting_memgrd_trshld, 0, SETTING_TYPE_UINT32 },
    { S_KEY_OTA_UPDATE_RESET_CONFIG, handle_setting_ota_upd_rescfg, 0, SETTING_TYPE_UINT16 },
    { S_KEY_SENSOR_OFFSET, handle_setting_sensor_offset, 0, SETTING_TYPE_FLOAT },
    { S_KEY_SENSOR_LINEAR_MULTIPLIER, handle_setting_sensor_linear_multiplier, 0, SETTING_TYPE_UINT32},
    { S_KEY_SENSOR_READ_INTERVAL, handle_setting_sensor_read_interval, 0, SETTING_TYPE_UINT16 },
    { S_KEY_SENSOR_SAMPLING_COUNT, handle_setting_sensor_samples, 0, SETTING_TYPE_UINT16 },
    { S_KEY_SENSOR_SAMPLING_INTERVAL, handle_setting_sensor_smp_int, 0, SETTING_TYPE_UINT16 },
    { S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, handle_setting_sensor_smp_deviate, 0, SETTING_TYPE_UINT16 },
};

/**
 * Routines
 */ 

/// @brief Initiate the SPIFFS
void init_filesystem();

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
esp_err_t generate_serial_number(char *serial_number);


/* Settings management functions */

/**
 * @brief: Set the result of a setting update operation, including error code and message
 */
static inline void set_result(setting_update_msg_t *out,
                              esp_err_t err,
                              const char *fmt, ...);

/**
 * @brief: Find a setting entry by its key
 */
static const setting_entry_t* find_setting(const char *key);

/**
 * @brief: Read the old value of a setting as a string
 */
static esp_err_t get_setting_as_string(const setting_entry_t *e,
                                   const char *ns,
                                   const char *key,
                                   setting_update_msg_t *out);

/**
 * @brief: Write setting value from cJSON to NVS
 */
static esp_err_t write_setting_value(const setting_entry_t *e,
                                 const char *ns,
                                 const char *key,
                                 const cJSON *v);

/**
 * @brief: Apply setting based on key and cJSON value
 */
esp_err_t apply_setting(const char *key, const cJSON *val, setting_update_msg_t *out);

/**
 * @brief: Get default size for a setting type
 */
static size_t setting_type_default_size(settings_type_t t, const setting_entry_t *e);

/**
 * @brief: Build cJSON object for a setting: { "<key>": { "value":..., "type":..., "size":... } }
 */
static cJSON *build_setting_payload_json(const setting_entry_t *e,
                                         const char *ns,
                                         setting_update_msg_t *msg_out);

/**
 * @brief Return cJSON object: { "<key>": { "value":..., "type":..., "size":... } }.
 */
cJSON *get_setting_value_JSON(const char *key, setting_update_msg_t *msg_out);

/** 
 * @brief Return cJSON object with all settings and their values 
 */
cJSON *get_all_settings_value_JSON(setting_update_msg_t *msg_out);

/* System Control routines*/
/**
 * @brief: System reboot
 */
esp_err_t system_reboot();

/**
 * @brief: Task to reboot the device in async mode
 */
void system_reboot_task(void *param);

/* OTA Update routines */
/**
 * @brief: Call OTA update
 */
esp_err_t perform_ota_update(const char *url);

/**
 * @brief: Generate the storage update URL
 */
esp_err_t generate_storage_update_url(const char *firmware_url, char **storage_url);

/**
 * @brief: Download and update the SPIFFS partition
 */
esp_err_t download_and_update_spiffs_partition(const char *url);

/**
 * @brief: Check if the OTA partitions are valid
 */
esp_err_t check_ota_partitions(void);

/**
 * @brief: OTA update task
 */
void ota_update_task(void *param);

/* System Control routines*/

/**
 * @brief: Reset factory settings
 */
esp_err_t reset_factory_settings();

/**
 * @brief: Reset device settings
 */
esp_err_t reset_device_settings();

/**
 * @brief: Reset WiFi settings
 */
esp_err_t reset_wifi_settings();


#endif