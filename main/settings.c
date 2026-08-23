#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"

#include <ctype.h>
#include "esp_spiffs.h"  // Include for SPIFFS
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "non_volatile_storage.h"
#include "ca_cert_manager.h"

#include "esp_wifi.h"

#include "settings.h"
#include "main.h"
#include "flags.h"
#include "sensor.h"
#include "web.h"

#define BUFFSIZE 1024
static char ota_write_data[BUFFSIZE + 1] = { 0 };
const size_t s_settings_count = sizeof(s_settings)/sizeof(s_settings[0]);

/*
 * Routines implementation
 */

/* System Init routines */

 /**
  * @brief Initialize filesystem for web server
  */
void init_filesystem() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information");
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

/**
 * @brief: Initiating settings
 *  - fill-in missing settings with default values
 *  - generate device serial number if not present
 *  Core routine to be called at the start of the application to ensure all settings are initialized and ready for use.
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t settings_init() {

    // reset device readiness by resetting BIT_DEVICE_READY bit
    xEventGroupClearBits(g_sys_events, BIT_DEVICE_READY);
    
    // defaults
    bool is_dynamically_allocated = false;

    // Parameter: sensor offset
    if (nvs_read_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, &sensor_data.voltage_offset) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %f", S_KEY_SENSOR_OFFSET, sensor_data.voltage_offset);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_OFFSET);
        sensor_data.voltage_offset = S_DEFAULT_SENSOR_OFFSET;
        if (nvs_write_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, sensor_data.voltage_offset) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %f", S_KEY_SENSOR_OFFSET, sensor_data.voltage_offset);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %f", S_KEY_SENSOR_OFFSET, sensor_data.voltage_offset);
            return ESP_FAIL;
        }
    }

    // Parameter: sensor linear multiplier
    if (nvs_read_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, &sensor_data.sensor_linear_multiplier) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %lu", S_KEY_SENSOR_LINEAR_MULTIPLIER, sensor_data.sensor_linear_multiplier);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_LINEAR_MULTIPLIER);
        sensor_data.sensor_linear_multiplier = S_DEFAULT_SENSOR_LINEAR_MULTIPLIER;
        if (nvs_write_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, sensor_data.sensor_linear_multiplier) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %lu", S_KEY_SENSOR_LINEAR_MULTIPLIER, sensor_data.sensor_linear_multiplier);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %lu", S_KEY_SENSOR_LINEAR_MULTIPLIER, sensor_data.sensor_linear_multiplier);
            return ESP_FAIL;
        }
    }

    // Parameter: MQTT connection mode
    mqtt_connection_mode_t mqtt_connection_mode;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_MQTT_CONNECT, mqtt_connection_mode);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_CONNECT);
        mqtt_connection_mode = S_DEFAULT_MQTT_CONNECT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, mqtt_connection_mode) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_MQTT_CONNECT, mqtt_connection_mode);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_MQTT_CONNECT, mqtt_connection_mode);
            return ESP_FAIL;
        }
    }  

    // Parameter: MQTT server
    char *mqtt_server = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_SERVER, mqtt_server);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_SERVER);
        mqtt_server = S_DEFAULT_MQTT_SERVER;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_SERVER, mqtt_server) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_SERVER, mqtt_server);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_SERVER, mqtt_server);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_server); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT port
    uint16_t mqtt_port;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_MQTT_PORT, mqtt_port);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PORT);
        mqtt_port = S_DEFAULT_MQTT_PORT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, mqtt_port) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_MQTT_PORT, mqtt_port);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_MQTT_PORT, mqtt_port);
            return ESP_FAIL;
        }
    }

    // Parameter: MQTT protocol
    char *mqtt_protocol = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_PROTOCOL, mqtt_protocol);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PROTOCOL);
        mqtt_protocol = S_DEFAULT_MQTT_PROTOCOL;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, mqtt_protocol) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_PROTOCOL, mqtt_protocol);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_PROTOCOL, mqtt_protocol);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_protocol); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT user
    char *mqtt_user = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_USER, mqtt_user);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_USER);
        mqtt_user = S_DEFAULT_MQTT_USER;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_USER, mqtt_user) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_USER, mqtt_user);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_USER, mqtt_user);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_user); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT password
    char *mqtt_password = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_PASSWORD, mqtt_password);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PASSWORD);
        mqtt_password = S_DEFAULT_MQTT_PASSWORD;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, mqtt_password) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_PASSWORD, mqtt_password);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_PASSWORD, mqtt_password);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_password); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT prefix
    char *mqtt_prefix = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_PREFIX, mqtt_prefix);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PREFIX);
        mqtt_prefix = S_DEFAULT_MQTT_PREFIX;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, mqtt_prefix) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_PREFIX, mqtt_prefix);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_PREFIX, mqtt_prefix);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_prefix); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: HomeAssistant MQTT Integration prefix
    char *ha_prefix = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_HA_PREFIX, ha_prefix);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_HA_PREFIX);
        ha_prefix = S_DEFAULT_HA_PREFIX;
        if (nvs_write_string(S_NAMESPACE, S_KEY_HA_PREFIX, ha_prefix) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_HA_PREFIX, ha_prefix);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_HA_PREFIX, ha_prefix);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(ha_prefix); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: Device ID (actually, MAC)
    char *device_id = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_DEVICE_ID, device_id);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_DEVICE_ID);

        char new_device_id[DEVICE_ID_LENGTH+1];
        uint8_t mac[6];  // Array to hold the MAC address
        ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));  // Use esp_read_mac to get the MAC address

        // Extract the last 3 bytes (6 characters in hexadecimal) of the MAC address
        snprintf(new_device_id, sizeof(new_device_id), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        if (nvs_write_string(S_NAMESPACE, S_KEY_DEVICE_ID, new_device_id) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_DEVICE_ID, new_device_id);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_DEVICE_ID, new_device_id);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(device_id); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: Device Serial (to be used for API)
    char *device_serial = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_DEVICE_SERIAL, device_serial);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_DEVICE_SERIAL);
        char new_device_serial[DEVICE_SERIAL_LENGTH+1];
        generate_serial_number(new_device_serial);       
        if (nvs_write_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, new_device_serial) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_DEVICE_SERIAL, new_device_serial);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_DEVICE_SERIAL, new_device_serial);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(device_serial); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: Sensor read interval
    uint16_t sensor_intervl;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &sensor_intervl) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_SENSOR_READ_INTERVAL, sensor_intervl);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_READ_INTERVAL);
        sensor_intervl = S_DEFAULT_SENSOR_READ_INTERVAL;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, sensor_intervl) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_SENSOR_READ_INTERVAL, sensor_intervl);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_SENSOR_READ_INTERVAL, sensor_intervl);
            return ESP_FAIL;
        }
    }

    // Parameter: Enable sensor sampling
    uint16_t sensor_smp_en;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, &sensor_smp_en) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_SENSOR_SAMPLING_ENABLE, sensor_smp_en);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_SAMPLING_ENABLE);
        sensor_smp_en = S_DEFAULT_SENSOR_SAMPLING_ENABLE;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, sensor_smp_en) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_SENSOR_SAMPLING_ENABLE, sensor_smp_en);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_SENSOR_SAMPLING_ENABLE, sensor_smp_en);
            return ESP_FAIL;
        }
    }

    // Parameter: Number of samples to collect per measurement
    uint16_t sensor_samples;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &sensor_samples) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_SENSOR_SAMPLING_COUNT, sensor_samples);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_SAMPLING_COUNT);
        sensor_samples = S_DEFAULT_SENSOR_SAMPLING_COUNT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, sensor_samples) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_SENSOR_SAMPLING_COUNT, sensor_samples);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_SENSOR_SAMPLING_COUNT, sensor_samples);
            return ESP_FAIL;
        }
    }

    // Parameter: Interval between samples in milliseconds
    uint16_t sensor_smp_int;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &sensor_smp_int) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_SENSOR_SAMPLING_INTERVAL, sensor_smp_int);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_SAMPLING_INTERVAL);
        sensor_smp_int = S_DEFAULT_SENSOR_SAMPLING_INTERVAL;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, sensor_smp_int) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_SENSOR_SAMPLING_INTERVAL, sensor_smp_int);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_SENSOR_SAMPLING_INTERVAL, sensor_smp_int);
            return ESP_FAIL;
        }
    }

    // Parameter: Threshold percentage for filtering
    uint16_t sensor_deviate;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, &sensor_deviate) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, sensor_deviate);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION);
        sensor_deviate = S_DEFAULT_SENSOR_SAMPLING_MEDIAN_DEVIATION;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, sensor_deviate) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, sensor_deviate);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, sensor_deviate);
            return ESP_FAIL;
        }
    }

    // Parameter: Update Home Assistant definitions every X minutes
    uint32_t ha_upd_intervl;
    if (nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %li", S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_HA_UPDATE_INTERVAL);
        ha_upd_intervl = S_DEFAULT_HA_UPDATE_INTERVAL;
        if (nvs_write_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %li", S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %li", S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl);
            return ESP_FAIL;
        }
    }

    // Parameter: OTA Update url
    char *ota_update_url = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_update_url) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_OTA_UPDATE_URL, ota_update_url);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_OTA_UPDATE_URL);
        ota_update_url = S_DEFAULT_OTA_UPDATE_URL;
        if (nvs_write_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, ota_update_url) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_OTA_UPDATE_URL, ota_update_url);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_OTA_UPDATE_URL, ota_update_url);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(ota_update_url); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: OTA Update reset config
    uint16_t ota_upd_rescfg;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_OTA_UPDATE_RESET_CONFIG, &ota_upd_rescfg) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_OTA_UPDATE_RESET_CONFIG, ota_upd_rescfg);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_OTA_UPDATE_RESET_CONFIG);
        ota_upd_rescfg = S_DEFAULT_OTA_UPDATE_RESET_CONFIG;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_OTA_UPDATE_RESET_CONFIG, ota_upd_rescfg) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_OTA_UPDATE_RESET_CONFIG, ota_upd_rescfg);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_OTA_UPDATE_RESET_CONFIG, ota_upd_rescfg);
            return ESP_FAIL;
        }
    }

    // Parameter: Network logging type
    uint16_t net_log_type;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_TYPE, &net_log_type) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_NET_LOGGING_TYPE, net_log_type);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_NET_LOGGING_TYPE);
        net_log_type = S_DEFAULT_NET_LOGGING_TYPE;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_TYPE, net_log_type) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_NET_LOGGING_TYPE, net_log_type);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_NET_LOGGING_TYPE, net_log_type);
            return ESP_FAIL;
        }
    }

    // Parameter: Network logging host
    char *net_log_host = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_NET_LOGGING_HOST, &net_log_host) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_NET_LOGGING_HOST, net_log_host);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_NET_LOGGING_HOST);
        net_log_host = S_DEFAULT_NET_LOGGING_HOST;
        if (nvs_write_string(S_NAMESPACE, S_KEY_NET_LOGGING_HOST, net_log_host) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_NET_LOGGING_HOST, net_log_host);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_NET_LOGGING_HOST, net_log_host);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(net_log_host); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: Network logging port
    uint16_t net_log_port;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_PORT, &net_log_port) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_NET_LOGGING_PORT, net_log_port);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_NET_LOGGING_PORT);
        net_log_port = S_DEFAULT_NET_LOGGING_PORT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_PORT, net_log_port) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_NET_LOGGING_PORT, net_log_port);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_NET_LOGGING_PORT, net_log_port);
            return ESP_FAIL;
        }
    }

    // Parameter: Network logging keep stdout
    uint16_t net_log_stdout;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_KEEP_STDOUT, &net_log_stdout) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_NET_LOGGING_KEEP_STDOUT, net_log_stdout);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_NET_LOGGING_KEEP_STDOUT);
        net_log_stdout = S_DEFAULT_NET_LOGGING_KEEP_STDOUT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_KEEP_STDOUT, net_log_stdout) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_NET_LOGGING_KEEP_STDOUT, net_log_stdout);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_NET_LOGGING_KEEP_STDOUT, net_log_stdout);
            return ESP_FAIL;
        }
    }

    // Parameter: Memory guard mode
    uint16_t memguard_mode;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, &memguard_mode) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_STATUS_MEMGUARD_MODE, memguard_mode);  
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_STATUS_MEMGUARD_MODE);
        memguard_mode = S_DEFAULT_STATUS_MEMGUARD_MODE;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, memguard_mode) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_STATUS_MEMGUARD_MODE, memguard_mode);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_STATUS_MEMGUARD_MODE, memguard_mode);
            return ESP_FAIL;
        }
    }
    // Parameter: Memory guard threshold
    uint32_t memgrd_trshld;
    if (nvs_read_uint32(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_THRESHOLD, &memgrd_trshld) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %li", S_KEY_STATUS_MEMGUARD_THRESHOLD, memgrd_trshld);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_STATUS_MEMGUARD_THRESHOLD);
        memgrd_trshld = S_DEFAULT_STATUS_MEMGUARD_THRESHOLD;
        if (nvs_write_uint32(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_THRESHOLD, memgrd_trshld) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %li", S_KEY_STATUS_MEMGUARD_THRESHOLD, memgrd_trshld);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %li", S_KEY_STATUS_MEMGUARD_THRESHOLD, memgrd_trshld);
            return ESP_FAIL;
        }
    }

    // device ready
    xEventGroupSetBits(g_sys_events, BIT_DEVICE_READY);
    
    return ESP_OK;

}

/**
 * @brief: Generate a random serial number consisting of uppercase letters and digits
 */
esp_err_t generate_serial_number(char *serial_number) {
    const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int i;

    for (i = 0; i < DEVICE_SERIAL_LENGTH; ++i) {
        int index = esp_random() % (sizeof(alphanum) - 1);  // esp_random generates a random 32-bit number
        serial_number[i] = alphanum[index];
    }

    serial_number[DEVICE_SERIAL_LENGTH] = '\0';  // Null-terminate the string
    return ESP_OK;
}

/* System Control routines */
/**
 * @brief: Reboot the device
 * 
 * This function reboots the device by calling the esp_restart() function.
 * 
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t system_reboot() {
    // call for the reboot task
    xTaskCreate(&system_reboot_task, "reboot_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}

/**
 * @brief: Task to reboot the device in async mode
 * 
 * This function creates a FreeRTOS task to reboot the device.
 * 
 * @param param: Pointer to the task parameters
 */
void system_reboot_task(void *param) {

    ESP_LOGI(TAG, "Reboot sequence task initiated");
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second

    // Stop the MQTT client
    if (mqtt_stop() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop the MQTT client");
    } else {
        ESP_LOGI(TAG, "MQTT client stopped");
    }

    // Stop the HTTP server
    if (http_stop() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop the HTTP server");
    } else {
        ESP_LOGI(TAG, "HTTP server stopped");
    }

    // Stop Wi-Fi
    esp_wifi_stop();
    esp_wifi_deinit();
    ESP_LOGI(TAG, "Wi-Fi stopped");


    ESP_LOGI(TAG, "Rebooting the device...");
    esp_restart();
    vTaskDelete(NULL);
}

/* Settings management functions */
/**
 * @brief: Read the old value of a setting as a string
 * 
 * @param e Pointer to the setting_entry_t structure
 * @param ns NVS namespace
 * @param key Setting key
 * @param[out] out Pointer to the setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t get_setting_as_string(const setting_entry_t *e,
                                   const char *ns,
                                   const char *key,
                                   setting_update_msg_t *out)
{
    if (!e || !out) return ESP_ERR_INVALID_ARG;
    out->has_old = false;
    out->old_value_str[0] = '\0';

    esp_err_t err;

    switch (e->type_t) {
    case SETTING_TYPE_UINT32: {
        uint32_t v = 0;
        err = nvs_read_uint32(ns, key, &v);
        if (err != ESP_OK) return err;
        snprintf(out->old_value_str, sizeof(out->old_value_str), "%"PRIu32, v);
        out->has_old = true;
        return ESP_OK;
    }
    case SETTING_TYPE_UINT16: {
        uint16_t v = 0;
        err = nvs_read_uint16(ns, key, &v);
        if (err != ESP_OK) return err;
        snprintf(out->old_value_str, sizeof(out->old_value_str), "%"PRIu16, v);
        out->has_old = true;
        return ESP_OK;
    }

    case SETTING_TYPE_STRING: {
        // nvs_read_string allocates; we must free
        char *buf_value = NULL;
        err = nvs_read_string(ns, key, &buf_value);
        if (err != ESP_OK) return err;

        // Compute effective max we are willing to store in out->old_value_str
        size_t cap = sizeof(out->old_value_str);
        if (e->max_str_size > 0 && e->max_str_size < cap) {
            cap = e->max_str_size;
        }

        // Ensure we always NUL-terminate inside out->old_value_str
        // cap is the maximum destination size we want to consider (including NUL)
        if (cap == 0) {
            free(buf_value);
            return ESP_ERR_INVALID_SIZE;
        }

        // Copy and truncate
        strlcpy(out->old_value_str, buf_value, cap);

        // If cap < sizeof(out->old_value_str), ensure remainder isn't stale (optional)
        if (cap < sizeof(out->old_value_str)) {
            out->old_value_str[cap - 1] = '\0';
        }

        free(buf_value);

        out->has_old = true;
        return ESP_OK;
    }
    case SETTING_TYPE_FLOAT: {
        float v = 0;
        err = nvs_read_float(ns, key, &v);
        if (err != ESP_OK) return err;
        snprintf(out->old_value_str, sizeof(out->old_value_str), "%f", (double)v);
        out->has_old = true;
        return ESP_OK;
    }
    case SETTING_TYPE_DOUBLE: {
        double v = 0;
        err = nvs_read_double(ns, key, &v);
        if (err != ESP_OK) return err;
        snprintf(out->old_value_str, sizeof(out->old_value_str), "%lf", v);
        out->has_old = true;
        return ESP_OK;
    }
    case SETTING_TYPE_BLOB:
        // blobs are tricky to “print”; you can report size/hash instead
        return ESP_ERR_NOT_SUPPORTED;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

/**
 * @brief: Write a new value for a setting
 * 
 * @param e Pointer to the setting_entry_t structure
 * @param ns NVS namespace
 * @param key Setting key
 * @param v New value as cJSON object
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t write_setting_value(const setting_entry_t *e,
                                 const char *ns,
                                 const char *key,
                                 const cJSON *v)
{
    if (!e || !key || !v) return ESP_ERR_INVALID_ARG;

    switch (e->type_t) {
    case SETTING_TYPE_UINT32:
        if (!cJSON_IsNumber(v)) return ESP_ERR_INVALID_ARG;
        return nvs_write_uint32(ns, key, (uint32_t)v->valuedouble);

    case SETTING_TYPE_UINT16:
        if (!cJSON_IsNumber(v)) return ESP_ERR_INVALID_ARG;
        return nvs_write_uint16(ns, key, (uint16_t)v->valuedouble);

    case SETTING_TYPE_STRING:
        if (!cJSON_IsString(v) || !v->valuestring) return ESP_ERR_INVALID_ARG;
        if (e->max_str_size > 0 && strlen(v->valuestring) >= e->max_str_size) return ESP_ERR_INVALID_SIZE;
        return nvs_write_string(ns, key, v->valuestring);

    case SETTING_TYPE_FLOAT:
        if (!cJSON_IsNumber(v)) return ESP_ERR_INVALID_ARG;
        return nvs_write_float(ns, key, (float)v->valuedouble);

    case SETTING_TYPE_DOUBLE:
        if (!cJSON_IsNumber(v)) return ESP_ERR_INVALID_ARG;
        return nvs_write_double(ns, key, (double)v->valuedouble);

    case SETTING_TYPE_BLOB:
        // You’d need a format (base64 string?) and then decode -> nvs_write_blob()
        return ESP_ERR_NOT_SUPPORTED;

    default:
        return ESP_ERR_INVALID_ARG;
    }
}

/**
 * @brief: Apply a single setting based on key-value pair
 * 
 * @param key: Setting key
 * @param val: Setting value (cJSON object)
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t apply_setting(const char *key,
                               const cJSON *val,
                               setting_update_msg_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    // Default
    set_result(out, ESP_OK, "OK");

    if (!key || !val) {
        set_result(out, ESP_ERR_INVALID_ARG, "Invalid key or val (NULL)");
        return ESP_ERR_INVALID_ARG;
    }

    const setting_entry_t *entry = find_setting(key);
    if (!entry) {
        // Forward-compatible behavior: ignore unknown keys but tell the user.
        set_result(out, ESP_ERR_NOT_FOUND, "Unknown setting key or setting is protected (read-only): %s", key);
        ESP_LOGW(TAG, "Attempt to update unknown or protected setting key: %s", key);
        return ESP_ERR_NOT_FOUND;
        // If you want strict mode instead, return ESP_ERR_INVALID_ARG here.
    }

    // read old (best-effort)
    esp_err_t rerr = get_setting_as_string(entry, S_NAMESPACE, key, out);
    if (rerr != ESP_OK) {
        // you can decide if failing to read old should block update or not
        // I'd log it but still attempt write.
        out->has_old = false;
        out->old_value_str[0] = '\0';
    }

    // optional per-setting validation / actions
    if (entry->handler) {
        esp_err_t herr = entry->handler(key, val, out);
        if (herr != ESP_OK) return herr;
        // if handler performs the write itself, you can stop here.
        // Otherwise let it fall through to generic write.
    } else {
        // no handler, proceed to generic write. let's notice it.
        ESP_LOGI(TAG, "No handler for setting '%s', proceeding to generic write", key);
    }

    esp_err_t werr = write_setting_value(entry, S_NAMESPACE, key, val);
    if (werr != ESP_OK) {
        set_result(out, werr, "Failed to write '%s': %s", key, esp_err_to_name(werr));
        return werr;
    } else {
        ESP_LOGI(TAG, "Successfully updated setting '%s'", key);
    }

    set_result(out, ESP_OK, "Updated setting '%s'%s%s%s",
               key,
               out->has_old ? " (was " : "",
               out->has_old ? out->old_value_str : "",
               out->has_old ? ")" : "");
    return ESP_OK;
}

/**
 * @brief: Set the result message and error code for a setting update
 * @param[out] out Pointer to the setting_update_msg_t structure to populate
 * @param[in] err Error code to set
 * @param[in] fmt Format string for the message
 * @param[in] ... Additional arguments for the format string
 * 
 * @note: If 'out' is NULL, the function does nothing
 */
static inline void set_result(setting_update_msg_t *out,
                              esp_err_t err,
                              const char *fmt, ...)
{
    if (!out) return;

    out->err_code = err;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out->msg, sizeof(out->msg), fmt, ap);
    va_end(ap);
}

/** 
 * @brief: Find setting entry by key
 * 
 * @param key Setting key to search for
 * @return Pointer to the setting_entry_t if found, NULL otherwise
 */
static const setting_entry_t* find_setting(const char *key)
{
    if (!key) return NULL;
    for (size_t i = 0; i < (sizeof(s_settings)/sizeof(s_settings[0])); i++) {
        if (strcmp(key, s_settings[i].key) == 0) {
            return &s_settings[i];
        }
    }
    return NULL;
}

/**
 * @brief: Get the default size for a setting type
 * 
 * @param t: Setting type
 * @param e: Pointer to the setting_entry_t structure (can be NULL)
 * 
 * @return Default size in bytes for the given setting type
 */
static size_t setting_type_default_size(settings_type_t t, const setting_entry_t *e)
{
    switch (t) {
    case SETTING_TYPE_UINT32: return sizeof(uint32_t);
    case SETTING_TYPE_UINT16: return sizeof(uint16_t);
    case SETTING_TYPE_FLOAT:  return sizeof(float);
    case SETTING_TYPE_DOUBLE: return sizeof(double);
    case SETTING_TYPE_STRING: return (e && e->max_str_size) ? e->max_str_size : 0;
    case SETTING_TYPE_BLOB:   return (e && e->max_str_size) ? e->max_str_size : 0; // interpret max_str_size as max blob bytes
    default:                  return 0;
    }
}

/**
 * @brief Build a cJSON payload for a setting entry
 * 
 * @param e Pointer to the setting_entry_t structure
 * @param ns NVS namespace
 * @param[out] msg_out Pointer to setting_update_msg_t structure to store the result
 * 
 * @return cJSON* representing the setting payload, or NULL on failure
 *
 * Caller owns returned cJSON* and must cJSON_Delete().
 */
static cJSON *build_setting_payload_json(const setting_entry_t *e,
                                         const char *ns,
                                         setting_update_msg_t *msg_out)
{
    if (!e || !e->key || !ns) {
        if (msg_out) set_result(msg_out, ESP_ERR_INVALID_ARG, "Invalid args");
        return NULL;
    }

    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        if (msg_out) set_result(msg_out, ESP_ERR_NO_MEM, "No memory for JSON payload");
        return NULL;
    }

    // Always include type + size (even if value missing)
    cJSON_AddNumberToObject(payload, "type", (int)e->type_t);
    cJSON_AddNumberToObject(payload, "max_size", (int)setting_type_default_size(e->type_t, e));

    esp_err_t err = ESP_OK;

    switch (e->type_t) {
    case SETTING_TYPE_UINT32: {
        uint32_t v = 0;
        err = nvs_read_uint32(ns, e->key, &v);
        if (err == ESP_OK) {
            // cJSON numbers are double; uint32 fits exactly up to 2^32-1? (not exactly for all values)
            // For safety, store as number if you know values stay small, or store as string.
            // We'll store as number (common for intervals, flags, etc.).
            cJSON_AddNumberToObject(payload, "value", (double)v);
            cJSON_AddNumberToObject(payload, "size", (int)setting_type_default_size(e->type_t, e));
        }
        break;
    }
    case SETTING_TYPE_UINT16: {
        uint16_t v = 0;
        err = nvs_read_uint16(ns, e->key, &v);
        if (err == ESP_OK) {
            cJSON_AddNumberToObject(payload, "value", (double)v);
            cJSON_AddNumberToObject(payload, "size", (int)setting_type_default_size(e->type_t, e));
        }
        break;
    }
    case SETTING_TYPE_STRING: {
        char *s = NULL;
        err = nvs_read_string(ns, e->key, &s); // allocates; you must free(s)
        if (err == ESP_OK) {
            if (!s) {
                // Unexpected but handle
                cJSON_AddStringToObject(payload, "value", "");
            } else {
                // Optionally enforce max_str_size at read time (truncate)
                if (e->max_str_size > 0 && strlen(s) >= e->max_str_size) {
                    // truncate in-place safely
                    s[e->max_str_size - 1] = '\0';
                }
                cJSON_AddStringToObject(payload, "value", s);
                cJSON_AddNumberToObject(payload, "size", (int)strlen(s) + 1); // include null terminator
            }
            free(s);
        }
        break;
    }
    case SETTING_TYPE_FLOAT: {
        float v = 0;
        err = nvs_read_float(ns, e->key, &v);   // if you have this wrapper
        if (err == ESP_OK) {
            cJSON_AddNumberToObject(payload, "value", (double)v);
            cJSON_AddNumberToObject(payload, "size", (int)setting_type_default_size(e->type_t, e));
        }
        break;
    }
    case SETTING_TYPE_DOUBLE: {
        double v = 0;
        err = nvs_read_double(ns, e->key, &v);  // if you have this wrapper
        if (err == ESP_OK) {
            cJSON_AddNumberToObject(payload, "value", v);
            cJSON_AddNumberToObject(payload, "size", (int)setting_type_default_size(e->type_t, e));
        }
        break;
    }
    default:
        err = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    if (err != ESP_OK) {
        // If key missing in NVS, you might decide to return value=null but still succeed.
        // Here we treat any read failure as failure; easy to adjust.
        if (msg_out) set_result(msg_out, err, "Failed to read '%s': %s", e->key, esp_err_to_name(err));
        cJSON_Delete(payload);
        return NULL;
    }

    if (msg_out) set_result(msg_out, ESP_OK, "OK");
    return payload;
}

/**
 * @brief Return cJSON object: { "<key>": { "value":..., "type":..., "size":... } }
 * 
 * @param key Setting key
 * @param[out] msg_out Pointer to setting_update_msg_t structure to store the result
 * 
 * @return cJSON* or NULL if key not found or read failed.
 *
 * Caller owns returned cJSON* and must cJSON_Delete().
 */
cJSON *get_setting_value_JSON(const char *key, setting_update_msg_t *msg_out)
{
    if (!key) {
        if (msg_out) set_result(msg_out, ESP_ERR_INVALID_ARG, "Key is NULL");
        return NULL;
    }

    const setting_entry_t *e = find_setting(key);
    if (!e) {
        if (msg_out) set_result(msg_out, ESP_ERR_NOT_FOUND, "Setting '%s' not found", key);
        return NULL; // per your requirement
    }

    cJSON *payload = build_setting_payload_json(e, S_NAMESPACE, msg_out);
    if (!payload) {
        // msg_out already filled
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(payload);
        if (msg_out) set_result(msg_out, ESP_ERR_NO_MEM, "No memory for JSON root");
        return NULL;
    }

    // root["key"] = payload
    cJSON_AddItemToObject(root, key, payload);
    return root;
}

/**
 * @brief Return:
 * {
 *   "total": <total settings>,
 *   "data": {
 *      "<key1>": {..payload..},
 *      "<key2>": {..payload..}
 *   }
 * }
 * @param[out] msg_out Pointer to setting_update_msg_t structure to store the result
 * 
 * @return cJSON* or NULL if no settings found/read failed.
 * 
 * Caller owns returned cJSON* and must cJSON_Delete().
 */
cJSON *get_all_settings_value_JSON(setting_update_msg_t *msg_out)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (msg_out) set_result(msg_out, ESP_ERR_NO_MEM, "No memory for JSON root");
        return NULL;
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        cJSON_Delete(root);
        if (msg_out) set_result(msg_out, ESP_ERR_NO_MEM, "No memory for JSON data");
        return NULL;
    }

    cJSON_AddItemToObject(root, "data", data);

    int total = 0;

    // Iterate your table
    extern const setting_entry_t s_settings[];
    extern const size_t s_settings_count; // define this in settings.c

    for (size_t i = 0; i < s_settings_count; i++) {
        const setting_entry_t *e = &s_settings[i];
        if (!e->key) continue;

        setting_update_msg_t tmp = {0};
        cJSON *payload = build_setting_payload_json(e, S_NAMESPACE, &tmp);

        // Decide policy:
        // - If a setting is absent/unreadable, you can skip it, or include it with value=null.
        // Here we skip unreadable ones but still count total settings in table.
        total++;

        if (!payload) {
            // Optional: include failure info in output instead of skipping:
            // cJSON *fail = cJSON_CreateObject();
            // cJSON_AddNullToObject(fail, "value");
            // cJSON_AddNumberToObject(fail, "type", (int)e->type_t);
            // cJSON_AddNumberToObject(fail, "size", (int)setting_type_default_size(e->type_t, e));
            // cJSON_AddStringToObject(fail, "error", tmp.msg[0] ? tmp.msg : "read failed");
            // cJSON_AddItemToObject(data, e->key, fail);
            continue;
        }

        cJSON_AddItemToObject(data, e->key, payload);
    }

    cJSON_AddNumberToObject(root, "total", total);

    if (msg_out) set_result(msg_out, ESP_OK, "OK");
    return root;
}


/* Definitions of setting-specific validation handlers as declared in settings.h:s_settings.handler */

/**
 * @brief: Handle Home Assistant auto-discovery interval setting validation handler
 * 
 * @param v: cJSON object containing the new interval value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_ha_upd_intervl(const char *key, const cJSON *v, setting_update_msg_t *out) {

    // check if the value is within allowed range (HA_UPDATE_INTERVAL_MIN and HA_UPDATE_INTERVAL_MAX)
    if (v->valueint < HA_UPDATE_INTERVAL_MIN || v->valueint > HA_UPDATE_INTERVAL_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "Home Assistant auto-discovery interval value out of range (%d - %d)", HA_UPDATE_INTERVAL_MIN, HA_UPDATE_INTERVAL_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief: Handle MQTT connection mode setting validation handler
 * 
 * @param v: cJSON object containing the new MQTT connection mode value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_mqtt_connect(const char *key, const cJSON *v, setting_update_msg_t *out) {

    int mode = (int)v->valuedouble;

    if (!mqtt_conn_mode_is_valid(mode)) {
        set_result(out, ESP_ERR_INVALID_ARG,
                   "mqtt_conn_mode invalid (%d). Allowed: %d..%d",
                   mode,
                   MQTT_CONN_MODE_DISABLE,
                   MQTT_CONN_MODE_AUTOCONNECT);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle MQTT port setting validation handler
 * 
 * @param v: cJSON object containing the new MQTT port value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_mqtt_port(const char *key, const cJSON *v, setting_update_msg_t *out) {

    // check if the value is within allowed range (HA_UPDATE_INTERVAL_MIN and HA_UPDATE_INTERVAL_MAX)
    if (v->valueint < 1 || v->valueint > 65535) {
        set_result(out, ESP_ERR_INVALID_ARG, "MQTT Port value out of range (1 - 65535)");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}


/**
 * @brief: Handle Network logging type setting validation handler
 * 
 * @param v: cJSON object containing the new network logging type value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_net_log_type(const char *key, const cJSON *v, setting_update_msg_t *out) {

    int type = (int)v->valuedouble;

    if (type < 0 || type > 3) {
        set_result(out, ESP_ERR_INVALID_ARG,
                   "net_log_type invalid (%d). Allowed: %d..%d",
                   type,
                   0,
                   3);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Network logging port setting validation handler
 * 
 * @param v: cJSON object containing the new network logging port value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_net_log_port(const char *key, const cJSON *v, setting_update_msg_t *out) {

    // check if the value is within allowed range (1 - 65535)
    if (v->valueint < 1 || v->valueint > 65535) {
        set_result(out, ESP_ERR_INVALID_ARG, "TCP/UDP Logging port value out of range (1 - 65535)");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief: Handle Network logging stdout setting validation handler
 * 
 * @param v: cJSON object containing the new network logging stdout value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_net_log_stdout(const char *key, const cJSON *v, setting_update_msg_t *out) {

    int type = (int)v->valuedouble;

    if (type < 0 || type > 1) {
        set_result(out, ESP_ERR_INVALID_ARG,
                   "net_log_stdout invalid (%d). Allowed: %d..%d",
                   type,
                   0,
                   1);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Memory Guard mode setting validation handler
 * 
 * @param v: cJSON object containing the new Memory Guard mode value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_memgrd_mode(const char *key, const cJSON *v, setting_update_msg_t *out) {

    int mode = (int)v->valuedouble;

    if (mode != MEMGRD_MODE_DISABLED && mode != MEMGRD_MODE_WARN && mode != MEMGRD_MODE_RESTART) {
        set_result(out, ESP_ERR_INVALID_ARG,
                   "memgrd_mode invalid (%d). Allowed: %d, %d, %d",
                   mode,
                   MEMGRD_MODE_DISABLED,
                   MEMGRD_MODE_WARN,
                   MEMGRD_MODE_RESTART);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Memory Guard threshold setting validation handler
 * 
 * @param v: cJSON object containing the new Memory Guard threshold value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_memgrd_trshld(const char *key, const cJSON *v, setting_update_msg_t *out) {

    int threshold = (int)v->valuedouble;

    if (threshold < MEMGUARD_THRESHOLD_MIN || threshold > MEMGUARD_THRESHOLD_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG,
                   "memgrd_trshld invalid (%d). Allowed: %d..%d",
                   threshold,
                   MEMGUARD_THRESHOLD_MIN,
                   MEMGUARD_THRESHOLD_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle OTA update reset config setting validation handler
 * 
 * @param v: cJSON object containing the new OTA update reset config value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_ota_upd_rescfg(const char *key, const cJSON *v, setting_update_msg_t *out) {

    int reset_config = (int)v->valuedouble;

    if (reset_config < OTA_UPDATE_RESET_CONFIG_MIN || reset_config > OTA_UPDATE_RESET_CONFIG_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG,
                   "ota_upd_rescfg invalid (%d). Allowed: %d..%d",
                   reset_config,
                   OTA_UPDATE_RESET_CONFIG_MIN,
                   OTA_UPDATE_RESET_CONFIG_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Sensor offset setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor offset value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_offset(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor offset must be a number");
        return ESP_ERR_INVALID_ARG;
    }

    // You can add range checks here if needed, e.g.:
    if (v->valuedouble < SENSOR_OFFSET_MIN || v->valuedouble > SENSOR_OFFSET_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "%s out of range (%f - %f)", key, SENSOR_OFFSET_MIN, SENSOR_OFFSET_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Sensor linear multiplier setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor linear multiplier value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_linear_multiplier(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor linear multiplier must be a number");
        return ESP_ERR_INVALID_ARG;
    }

    // You can add range checks here if needed, e.g.:
    if (v->valuedouble < SENSOR_LINEAR_MULTIPLIER_MIN || v->valuedouble > SENSOR_LINEAR_MULTIPLIER_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "%s out of range (%f - %f)", key, SENSOR_LINEAR_MULTIPLIER_MIN, SENSOR_LINEAR_MULTIPLIER_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Sensor read interval setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor read interval value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_read_interval(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor read interval must be a number");
        return ESP_ERR_INVALID_ARG;
    }

    // You can add range checks here if needed, e.g.:
    if (v->valuedouble < SENSOR_READ_INTERVAL_MIN || v->valuedouble > SENSOR_READ_INTERVAL_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "%s out of range (%d - %d)", key, SENSOR_READ_INTERVAL_MIN, SENSOR_READ_INTERVAL_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure that the read interval is a multiple of the sampling interval if sampling is enabled
    // 1 - Check if sampling is enabled
    uint16_t sampling_enabled = 0;
    esp_err_t err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, &sampling_enabled);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_ENABLE, esp_err_to_name(err));
        return err; 
    }

    // 2 - Get sampling interval and sampling count
    uint16_t sampling_count = 0;
    uint16_t sampling_interval = 0;
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &sampling_interval);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_INTERVAL, esp_err_to_name(err));
        return err;
    }
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &sampling_count);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_COUNT, esp_err_to_name(err));
        return err;
    }

    // 3 - If sampling is enabled, check if read interval bigger than sampling interval * sampling count
    if (sampling_enabled) {
        uint32_t min_read_interval = sampling_interval * sampling_count;
        if (v->valuedouble < min_read_interval) {
            set_result(out, ESP_ERR_INVALID_ARG, "Sensor read interval must be greater than or equal to sampling interval * sampling count (%d)", min_read_interval);
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Sensor sampling enable setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor sampling enable value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_smp_en(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor sampling enable must be a number");
        return ESP_ERR_INVALID_ARG;
    }   

    if (v->valueint < SENSOR_SAMPLING_ENABLE_MIN || v->valueint > SENSOR_SAMPLING_ENABLE_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor sampling enable must be between %d and %d", SENSOR_SAMPLING_ENABLE_MIN, SENSOR_SAMPLING_ENABLE_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}


/**
 * @brief: Handle Sensor samples setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor samples value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_samples(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor samples must be a number");
        return ESP_ERR_INVALID_ARG;
    }

    // You can add range checks here if needed, e.g.:
    if (v->valuedouble < SENSOR_SAMPLING_COUNT_MIN || v->valuedouble > SENSOR_SAMPLING_COUNT_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "%s out of range (%d - %d)", key, SENSOR_SAMPLING_COUNT_MIN, SENSOR_SAMPLING_COUNT_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure that the read interval is a multiple of the sampling interval if sampling is enabled
    // 1 - Check if sampling is enabled
    uint16_t sampling_enabled = 0;
    esp_err_t err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, &sampling_enabled);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_ENABLE, esp_err_to_name(err));
        return err; 
    }

    // 2 - Get sampling interval and sampling count
    uint16_t sampling_count = v->valueint;
    uint16_t sampling_interval = 0;
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &sampling_interval);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_INTERVAL, esp_err_to_name(err));
        return err;
    }

    // 3 - If sampling is enabled, check if read interval bigger than sampling interval * sampling count
    if (sampling_enabled) {
        // Get the current read interval from NVS to compare
        uint16_t read_interval = 0;
        err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &read_interval);
        if (err != ESP_OK) {
            set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_READ_INTERVAL, esp_err_to_name(err));
            return err;
        }
        uint16_t min_read_interval = sampling_interval * sampling_count;
        if (read_interval < min_read_interval) {
            set_result(out, ESP_ERR_INVALID_ARG, "Sensor read interval must be greater than or equal to sampling interval * sampling count (%d)", min_read_interval);
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Sensor sampling interval setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor sampling interval value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_smp_int(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor sampling interval must be a number");
        return ESP_ERR_INVALID_ARG;
    }

    // You can add range checks here if needed, e.g.:
    if (v->valuedouble < SENSOR_SAMPLING_INTERVAL_MIN || v->valuedouble > SENSOR_SAMPLING_INTERVAL_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "%s out of range (%d - %d)", key, SENSOR_SAMPLING_INTERVAL_MIN, SENSOR_SAMPLING_INTERVAL_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure that the read interval is a multiple of the sampling interval if sampling is enabled
    // 1 - Check if sampling is enabled
    uint16_t sampling_enabled = 0;
    esp_err_t err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, &sampling_enabled);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_ENABLE, esp_err_to_name(err));
        return err; 
    }

    // 2 - Get sampling interval and sampling count
    uint16_t sampling_count = 0;
    uint16_t sampling_interval = v->valueint;
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &sampling_count);
    if (err != ESP_OK) {
        set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_SAMPLING_COUNT, esp_err_to_name(err));
        return err;
    }

    // 3 - If sampling is enabled, check if read interval bigger than sampling interval * sampling count
    if (sampling_enabled) {
        // Get the current read interval from NVS to compare
        uint16_t read_interval = 0;
        err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &read_interval);
        if (err != ESP_OK) {
            set_result(out, err, "Failed to read %s: %s", S_KEY_SENSOR_READ_INTERVAL, esp_err_to_name(err));
            return err;
        }
        uint16_t min_read_interval = sampling_interval * sampling_count;
        if (read_interval < min_read_interval) {
            set_result(out, ESP_ERR_INVALID_ARG, "Sensor read interval must be greater than or equal to sampling interval * sampling count (%d)", min_read_interval);
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK; // generic writer will store it
}

/**
 * @brief: Handle Sensor sampling median deviation setting validation handler
 * 
 * @param v: cJSON object containing the new Sensor sampling median deviation value
 * @param[out] out: Pointer to setting_update_msg_t structure to store the result
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
static esp_err_t handle_setting_sensor_smp_deviate(const char *key, const cJSON *v, setting_update_msg_t *out) {
    if (!cJSON_IsNumber(v)) {
        set_result(out, ESP_ERR_INVALID_ARG, "Sensor sampling median deviation must be a number");
        return ESP_ERR_INVALID_ARG;
    }

    // You can add range checks here if needed, e.g.:
    if (v->valuedouble < SENSOR_SAMPLING_MEDIAN_DEVIATION_MIN || v->valuedouble > SENSOR_SAMPLING_MEDIAN_DEVIATION_MAX) {
        set_result(out, ESP_ERR_INVALID_ARG, "%s out of range (%d - %d)", key, SENSOR_SAMPLING_MEDIAN_DEVIATION_MIN, SENSOR_SAMPLING_MEDIAN_DEVIATION_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // generic writer will store it
}

/* OTA Update routines */
/**
 * @brief Perform an OTA update from the given URL.
 *
 * This function initiates an OTA update process using the URL provided.
 * It configures the OTA client, downloads the firmware, and flashes it to the device.
 * Upon successful completion, the device will automatically reboot.
 *
 * @param url The URL of the firmware binary to download.
 * @return ESP_OK if the OTA update was successful, or an error code otherwise.
 */
esp_err_t perform_ota_update(const char *url) {
    ESP_LOGI(OTA_TAG, "Starting OTA update from URL: %s", url);
    
    char *ca_cert = NULL;
    
    // Load the CA root certificate
    if (load_ca_certificate(&ca_cert, CA_CERT_PATH_HTTPS) != ESP_OK) {
        ESP_LOGW(OTA_TAG, "Failed to load CA certificate. Proceeding without it.");
    }

    // Configure HTTP client for OTA update
    esp_http_client_config_t ota_https_client_config = {
        .url = url,
        .timeout_ms = 5000,  // Timeout for OTA update
        .cert_pem = ca_cert  // Set CA certificate if loaded, otherwise NULL
    };

    // Configure OTA update
    esp_https_ota_config_t ota_config = {
        .http_config = &ota_https_client_config,
    };

    // Start the OTA update process
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA firmware image update successful!");
    } else {
        ESP_LOGE(OTA_TAG, "OTA firmware image update failed: %s", esp_err_to_name(ret));
    }

    // Free the CA certificate memory if it was dynamically allocated
    if (ca_cert) {
        free(ca_cert);
    }

    return ret;
}

/**
 * @brief Generate OTA Storage Update URL based on the OTA Firmware Update URL.
 *
 * This function extracts the base path from the OTA Firmware Update URL and appends "Storage.bin".
 *
 * @param firmware_url The OTA Firmware Update URL.
 * @param[out] storage_url Pointer to dynamically allocated string containing the OTA Storage Update URL.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if inputs are invalid, ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t generate_storage_update_url(const char *firmware_url, char **storage_url) {
    if (firmware_url == NULL || storage_url == NULL) {
        ESP_LOGE(OTA_TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Find the last '/' in the firmware URL to extract the base path
    const char *last_slash = strrchr(firmware_url, '/');
    if (last_slash == NULL) {
        ESP_LOGE(OTA_TAG, "Malformed URL: %s", firmware_url);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate the base URL length (everything up to and including the last '/')
    size_t base_length = last_slash - firmware_url + 1;

    // Allocate memory for the new URL (base length + OTA_STORAGE_IMAGE_NAME + null terminator)
    *storage_url = malloc(base_length + strlen(OTA_STORAGE_IMAGE_NAME) + 1);
    if (*storage_url == NULL) {
        ESP_LOGE(OTA_TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    // Copy the base URL and append OTA_STORAGE_IMAGE_NAME
    strncpy(*storage_url, firmware_url, base_length);
    strcpy(*storage_url + base_length, OTA_STORAGE_IMAGE_NAME);

    ESP_LOGI(OTA_TAG, "Generated Storage Update URL: %s", *storage_url);
    return ESP_OK;
}

/**
 * @brief Download and update the SPIFFS partition with the data from the given URL.
 *
 * This function downloads the partition image from the given URL and writes it to the SPIFFS.
 *
 * @param url The URL of the storage partition image to download.
 * @return ESP_OK if the SPIFFS partition was updated successfully, or an error code otherwise.
 */
esp_err_t download_and_update_spiffs_partition(const char *url)
{
    ESP_LOGI(OTA_TAG, "Starting OTA storage update from URL: %s", url);

    esp_partition_t *spiffs_partition = NULL;
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
        NULL
    );

    if (it == NULL) {
        ESP_LOGE(OTA_TAG, "SPIFFS partition not found");
        return ESP_FAIL;
    }

    spiffs_partition = (esp_partition_t *)esp_partition_get(it);
    if (spiffs_partition == NULL) {
        ESP_LOGE(OTA_TAG, "Failed to get SPIFFS partition");
        esp_partition_iterator_release(it);
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "SPIFFS: partition type = %d", spiffs_partition->type);
    ESP_LOGI(OTA_TAG, "SPIFFS: partition subtype = %d", spiffs_partition->subtype);
    ESP_LOGI(OTA_TAG, "SPIFFS: partition starting address = 0x%lx", spiffs_partition->address);
    ESP_LOGI(OTA_TAG, "SPIFFS: partition size = %li", spiffs_partition->size);
    ESP_LOGI(OTA_TAG, "SPIFFS: partition label = %s", spiffs_partition->label);
    ESP_LOGI(OTA_TAG, "SPIFFS: partition encrypted = %d", spiffs_partition->encrypted);

    esp_partition_iterator_release(it);

    char *ca_cert = NULL;
    if (load_ca_certificate(&ca_cert, CA_CERT_PATH_HTTPS) != ESP_OK) {
        ESP_LOGW(OTA_TAG, "Failed to load CA certificate. Proceeding without it.");
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .cert_pem = ca_cert,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(OTA_TAG, "Failed to initialize HTTP client");
        free(ca_cert);
        return ESP_FAIL;
    }

    /*
     * Step 1: HEAD request to verify the object exists
     */
    esp_http_client_set_method(client, HTTP_METHOD_HEAD);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to open HEAD HTTP connection: %s", esp_err_to_name(err));
        free(ca_cert);
        esp_http_client_cleanup(client);
        return err;
    }
    ESP_LOGI(OTA_TAG, "SPIFFS OTA: HEAD HTTP connection opened successfully");

    int64_t head_len = esp_http_client_fetch_headers(client);
    if (head_len < 0) {
        ESP_LOGE(OTA_TAG, "Failed to fetch HEAD HTTP headers");
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(OTA_TAG, "SPIFFS OTA: HEAD HTTP headers fetched successfully");

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(OTA_TAG, "File not found at URL: %s, HTTP status=%d",
                 url, esp_http_client_get_status_code(client));
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(OTA_TAG, "SPIFFS OTA: File found at URL: %s", url);

    esp_http_client_cleanup(client);
    client = NULL;

    /*
     * Step 2: GET request to download the file
     */
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(OTA_TAG, "Failed to reinitialize HTTP client");
        free(ca_cert);
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to open GET HTTP connection: %s", esp_err_to_name(err));
        free(ca_cert);
        esp_http_client_cleanup(client);
        return err;
    }
    ESP_LOGI(OTA_TAG, "SPIFFS OTA: GET HTTP connection opened successfully");

    int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(OTA_TAG, "Failed to fetch GET HTTP headers");
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(OTA_TAG, "GET failed for URL: %s, HTTP status=%d",
                 url, esp_http_client_get_status_code(client));
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "SPIFFS OTA: GET headers fetched successfully, content_length=%lld",
             content_length);

    if (content_length <= 0) {
        ESP_LOGE(OTA_TAG, "Invalid content length: %lld", content_length);
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    /*
     * Step 3: Read first chunk BEFORE erasing partition
     */
    int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
    if (data_read < 0) {
        ESP_LOGE(OTA_TAG, "Error reading first chunk from HTTP stream");
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (data_read == 0) {
        ESP_LOGE(OTA_TAG, "HTTP stream returned 0 bytes, aborting before erasing SPIFFS");
        free(ca_cert);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    /*
     * Step 4: Erase only after we know we really have data
     */
    err = esp_partition_erase_range(spiffs_partition, 0, spiffs_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to erase SPIFFS partition: %s", esp_err_to_name(err));
        free(ca_cert);
        esp_http_client_cleanup(client);
        return err;
    }
    ESP_LOGI(OTA_TAG, "SPIFFS partition erased successfully");

    /*
     * Step 5: Write first chunk and continue streaming
     */
    int binary_file_length = 0;
    int offset = 0;

    ESP_LOGI(OTA_TAG, "Writing to SPIFFS partition:");

    while (1) {
        if (data_read > 0) {
            err = esp_partition_write(spiffs_partition, offset, ota_write_data, data_read);
            if (err != ESP_OK) {
                ESP_LOGE(OTA_TAG, "Failed to write SPIFFS partition: %s", esp_err_to_name(err));
                free(ca_cert);
                esp_http_client_cleanup(client);
                return err;
            }

            offset += data_read;
            binary_file_length += data_read;
            printf(".");
        } else if (data_read == 0) {
            printf("DONE\n");
            ESP_LOGI(OTA_TAG, "Connection closed, all data received");
            break;
        }

        data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0) {
            ESP_LOGE(OTA_TAG, "Error: SSL/HTTP data read error");
            free(ca_cert);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(OTA_TAG, "Total written binary data length: %d", binary_file_length);

    if (binary_file_length <= 0) {
        ESP_LOGE(OTA_TAG, "No data written to SPIFFS partition");
        err = ESP_FAIL;
    } else if (content_length > 0 && binary_file_length != content_length) {
        ESP_LOGE(OTA_TAG, "Incomplete SPIFFS image: expected %lld bytes, wrote %d bytes",
                 content_length, binary_file_length);
        err = ESP_FAIL;
    } else {
        ESP_LOGI(OTA_TAG, "SPIFFS partition update completed successfully");
        err = ESP_OK;
    }

    esp_http_client_cleanup(client);
    free(ca_cert);
    return err;
}

/**
 * @file: settings.c
 * @brief Check the OTA partitions for the current running partition and its state.
 * 
 * This function checks the OTA partitions to determine the current running partition
 * and its state. It prints the running partition label and the OTA state to the console.
 * 
 * @return ESP_OK if the OTA partitions were checked successfully, or an error code otherwise.
 */
esp_err_t check_ota_partitions(void) {

    ESP_LOGI(OTA_TAG, "Checking OTA partitions");

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(OTA_TAG, "Running partition not found!");
        return ESP_ERR_OTA_BASE;
    } else {
        ESP_LOGI(OTA_TAG, "Running from partition: %s", running->label);
    }

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA partition state: %d", ota_state);
    } else {
        ESP_LOGE(OTA_TAG, "Failed to get OTA state");
    }

    return ESP_OK;
}

/**
 * @brief: OTA update task
 * 
 * @param param: Pointer to the OTA update parameters
 * 
 * @note: This function should be called as a FreeRTOS task
 */
void ota_update_task(void *param) {

#if _DEVICE_ENABLE_STATUS_MEMGUARD
    // Store current memory guard mode
    uint16_t current_memguard_mode = S_DEFAULT_STATUS_MEMGUARD_MODE;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, &current_memguard_mode) != ESP_OK) {
        ESP_LOGW(OTA_TAG, "Failed to read current memory guard mode from NVS");
    } else {
        ESP_LOGD(OTA_TAG, "Current memory guard mode: %i", current_memguard_mode);
    }
    // Disable memory guard for OTA update task
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, MEMGRD_MODE_DISABLED)); // Disable memguard
    ESP_LOGI(OTA_TAG, "Memory guard disabled for OTA update task");
#endif
    ota_update_param_t *update_param = (ota_update_param_t *)param;
    esp_err_t ret = perform_ota_update(update_param->ota_url);
    
    // Check the result and handle any post-update logic
    if (ret == ESP_OK) {
        if(DO_OTA_STORAGE_UPDATE) {
            ESP_LOGI(OTA_TAG, "Performing SPIFFS image update after OTA firmware image update...");
            char *storage_url = NULL;
            if (generate_storage_update_url(update_param->ota_url, &storage_url) == ESP_OK) {
                ESP_LOGI(OTA_TAG, "SPIFFS Update URL: %s", storage_url);
                ret = download_and_update_spiffs_partition(storage_url);
                if (ret == ESP_OK) {
                    ESP_LOGI(OTA_TAG, "SPIFFS partition updated successfully.");
                } else {
                    ESP_LOGE(OTA_TAG, "Failed to update SPIFFS partition: %s", esp_err_to_name(ret));
                }
                free(storage_url);
            } else {
                ESP_LOGE(OTA_TAG, "Failed to generate SPIFFS Update URL");
            }
        }
#if _DEVICE_ENABLE_STATUS_MEMGUARD
        // Restore previous memory guard mode
        ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, current_memguard_mode));
        ESP_LOGI(OTA_TAG, "Memory guard mode restored to %i", current_memguard_mode);

#endif
        // Reset device configuration if required
        // 1 - get reset mode from NV
        uint16_t ota_upd_rescfg = S_DEFAULT_OTA_UPDATE_RESET_CONFIG;
        if (nvs_read_uint16(S_NAMESPACE, S_KEY_OTA_UPDATE_RESET_CONFIG, &ota_upd_rescfg) != ESP_OK) {
            ESP_LOGW(OTA_TAG, "Failed to read OTA update reset config from NVS, using default: %i", ota_upd_rescfg);
        } else {
            ESP_LOGI(OTA_TAG, "OTA update reset config: %i", ota_upd_rescfg);
        }
        // 2 - perform reset based on mode
        if (ota_upd_rescfg > 0) {
            // reset device settings
            if (reset_device_settings() != ESP_OK) {
                ESP_LOGE(OTA_TAG, "Failed to reset device settings after OTA update");
            } else {
                ESP_LOGI(OTA_TAG, "Device settings reset successfully after OTA update");   
            }           
        }
        // Reboot the device to apply the update
        system_reboot();
    } else {
        ESP_LOGE(OTA_TAG, "OTA update failed with error code: %s", esp_err_to_name(ret));
#if _DEVICE_ENABLE_STATUS_MEMGUARD
        // Restore previous memory guard mode
        ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, current_memguard_mode));
        ESP_LOGI(OTA_TAG, "Memory guard mode restored to %i", current_memguard_mode);
#endif
    }

    free(update_param);  // Free the allocated memory for parameters
    vTaskDelete(NULL);   // Delete the OTA update task
}

/* System control routines */
/**
 * @file settings.c
 * @brief Helper function to clear all data in NVS (factory reset)
 * 
 * This file contains the implementation of a helper function that performs
 * a factory reset by clearing all data stored in the Non-Volatile Storage (NVS).
 * 
 * @note This operation will erase all user settings and data stored in NVS.
 */
esp_err_t reset_factory_settings() {
    esp_err_t ret = nvs_flash_erase();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Factory reset: all data erased from NVS.");
    } else {
        ESP_LOGE(TAG, "Failed to erase NVS for factory reset: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief Helper function to clear all keys in the "settings" namespace.
 *
 * This function iterates through all keys stored in the "settings" namespace
 * and removes them. It is useful for resetting the settings to their default
 * state.
 */
esp_err_t reset_device_settings() {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(S_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_erase_all(handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Device settings reset: all keys erased in the '%s' namespace.", S_NAMESPACE);
        } else {
            ESP_LOGE(TAG, "Failed to erase '%s' namespace: %s", esp_err_to_name(ret), S_NAMESPACE);
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open '%s' namespace: %s", esp_err_to_name(ret), S_NAMESPACE);
    }
    return ret;
}

/**
 * @brief Resets the Wi-Fi settings to their default values.
 *
 * This function resets the Wi-Fi settings stored in the non-volatile storage
 * to their default values. It is typically used to clear any existing Wi-Fi
 * configurations and start fresh.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 */
esp_err_t reset_wifi_settings() {
    esp_err_t ret;
    nvs_handle_t wifi_handle;
    ret = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &wifi_handle); // Assuming Wi-Fi settings are stored in WIFI_NAMESPACE namespace

    if (ret == ESP_OK) {
        ret = nvs_erase_all(wifi_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Wi-Fi settings reset: all keys erased in the '%s' namespace.", WIFI_NAMESPACE);
        } else {
            ESP_LOGE(TAG, "Failed to erase '%s' namespace: %s", esp_err_to_name(ret), WIFI_NAMESPACE);
        }
        nvs_close(wifi_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open '%s' namespace: %s", esp_err_to_name(ret), WIFI_NAMESPACE);
    }

    return ret;
}