#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"

#include "settings.h"
#include "sensor.h"
#include "non_volatile_storage.h"

int device_ready = 0;

/*
 * Routines implementation
 */
esp_err_t settings_init() {

    // reset device readiness
    device_ready = 0;
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
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_DEVICE_ID);

        char new_device_id[DEVICE_ID_LENGTH+1];
        uint8_t mac[6];  // Array to hold the MAC address
        esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Use esp_read_mac to get the MAC address

        // Extract the last 3 bytes (6 characters in hexadecimal) of the MAC address
        snprintf(device_id, sizeof(new_device_id), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        if (nvs_write_string(S_NAMESPACE, S_KEY_DEVICE_ID, new_device_id) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_DEVICE_ID, new_device_id);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_DEVICE_ID, new_device_id);
            return ESP_FAIL;
        }
    }
    // free(device_id); // for string (char*) params only

    // Parameter: Device Serial (to be used for API)
    char *device_serial = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_DEVICE_SERIAL, device_serial);
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
    // free(device_serial); // for string (char*) params only

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

    // device ready
    device_ready = 1;
    
    return ESP_OK;

}

void generate_serial_number(char *serial_number) {
    const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int i;

    for (i = 0; i < DEVICE_SERIAL_LENGTH; ++i) {
        int index = esp_random() % (sizeof(alphanum) - 1);  // esp_random generates a random 32-bit number
        serial_number[i] = alphanum[index];
    }

    serial_number[DEVICE_SERIAL_LENGTH] = '\0';  // Null-terminate the string
}