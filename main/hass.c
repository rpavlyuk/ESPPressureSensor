#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "cJSON.h"

#include "wifi.h"
#include "hass.h"
#include "non_volatile_storage.h"
#include "settings.h"

/**
 * @brief: Initialize the device entity
 */
esp_err_t ha_device_init(ha_device_t *device) {
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;  // Handle NULL input
    }

    // Assign manufacturer
    device->manufacturer = strdup(HA_DEVICE_MANUFACTURER);  // strdup allocates memory and copies string
    if (device->manufacturer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for manufacturer");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "DEVICE: assigned manufacturer: %s", device->manufacturer);

    // Assign model
    device->model = strdup(HA_DEVICE_MODEL);
    if (device->model == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for model");
        free(device->manufacturer);  // Cleanup previously allocated memory
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "DEVICE: assigned model: %s", device->model);

    // Assign configuration URL (based on IP address)
    device->configuration_url = (char *)malloc(CFG_URL_LEN);
    if (device->configuration_url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for configuration_url");
        free(device->manufacturer);
        free(device->model);
        return ESP_ERR_NO_MEM;
    }
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(esp_netif_sta, &ip_info) == ESP_OK) {
        sprintf(device->configuration_url, "http://%d.%d.%d.%d/", IP2STR(&ip_info.ip));
    }
    ESP_LOGD(TAG, "DEVICE: assigned configuration_url: %s", device->configuration_url);

    // Assign device name
    device->name = NULL;
    esp_err_t err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device->name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device name from NVS");
        free(device->manufacturer);
        free(device->model);
        free(device->configuration_url);
        return err;
    }
    ESP_LOGD(TAG, "DEVICE: assigned name: %s", device->name);

    // Assign via_device (set to an empty string)
    device->via_device = strdup("");
    if (device->via_device == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for via_device");
        free(device->manufacturer);
        free(device->model);
        free(device->configuration_url);
        free(device->name);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGD(TAG, "DEVICE: assigned via_device: %s", device->via_device);

    // Assign identifiers[0]
    device->identifiers[0] = NULL;
    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device->identifiers[0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device serial from NVS");
        free(device->manufacturer);
        free(device->model);
        free(device->configuration_url);
        free(device->name);
        free(device->via_device);
        return err;
    }
    ESP_LOGD(TAG, "DEVICE: assigned identifiers[0]: %s", device->identifiers[0]);

    return ESP_OK;
}

/**
 * @brief: Destroy the device entity and free up memory
 */
esp_err_t ha_device_free(ha_device_t *device) {
    if (device != NULL) {
        free(device->manufacturer);
        free(device->model);
        free(device->configuration_url);
        free(device->name);
        free(device->via_device);
        free(device->identifiers[0]);  // Assuming only 1 identifier
        free(device);
    }
    return ESP_OK;

}

/**
 * @brief: Convert device entity to JSON
 */
cJSON *ha_device_to_JSON(ha_device_t *device) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "configuration_url", device->configuration_url);
    cJSON_AddStringToObject(root, "manufacturer", device->manufacturer);
    cJSON_AddStringToObject(root, "model", device->model);
    cJSON_AddStringToObject(root, "name", device->name);
    cJSON_AddStringToObject(root, "via_device", device->via_device);
    if (device->identifiers[0]) {
        cJSON_AddItemToObject(root, "identifiers", cJSON_CreateStringArray(device->identifiers, 1));
    }

    return root;
}

/**
 * @brief: Present device entity as string 
 */
char *ha_device_to_string(ha_device_t *device) {

    char buf[512];
    memset(buf, 0, sizeof(buf));
    char ident[256];
    memset(ident, 0, sizeof(ident));


    sprintf(ident, "[ %s ]", device->identifiers[0] ? device->identifiers[0] : "");


    sprintf(buf, "> DEVICE:\n- manufacturer: %s\n-model: %s\n-name: %s\n-via_device: %s\n-configuration_url: %s\n-identifiers: %s",
                    device->manufacturer, 
                    device->model,
                    device->name,
                    device->via_device,
                    device->configuration_url,
                    ident
                );   

    return buf;

}

/**
 * @brief: Initialize device availability entity. The entity contains one property with path of the "online" status topic.
 */
esp_err_t ha_availability_init(ha_entity_availability_t *availability) {
    if (availability == NULL) {
        return ESP_ERR_INVALID_ARG;  // Handle NULL input
    }

    // Allocate and read the MQTT prefix
    char *mqtt_prefix = NULL;
    esp_err_t err = nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT prefix from NVS");
        return err;
    }

    // Allocate and read the device ID
    char *device_id = NULL;
    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID from NVS");
        free(mqtt_prefix);  // Clean up previous allocation
        return err;
    }

    // Allocate memory for the availability topic
    size_t topic_len = strlen(mqtt_prefix) + strlen(device_id) + strlen(HA_DEVICE_STATUS_PATH) + 3;  // 3 for 2 slashes + null terminator
    availability->topic = (char *)malloc(topic_len);
    if (availability->topic == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for availability topic");
        free(mqtt_prefix);
        free(device_id);
        return ESP_ERR_NO_MEM;
    }

    // Construct the availability topic
    snprintf(availability->topic, topic_len, "%s/%s/%s", mqtt_prefix, device_id, HA_DEVICE_STATUS_PATH);
    ESP_LOGD(TAG, "DISCOVERY::AVAILABILITY: assigned availability topic: %s", availability->topic);

    // Clean up temporary variables
    free(mqtt_prefix);
    free(device_id);

    return ESP_OK;
}

/**
 * @brief: Destroy the device availability entity and free up memory
 */
esp_err_t ha_availability_free(ha_entity_availability_t *availability) {

    if (availability != NULL) {
        free(availability->topic);  // Free the availability topic
        free(availability);
    }

    return ESP_OK;
}

/**
 * @brief: Convert availability entity to JSON
 */
cJSON *ha_availability_to_JSON(ha_entity_availability_t *availability) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "topic", availability->topic);
    return root;
}

/**
 * @brief: Initialize device origin entity.
 */
esp_err_t ha_origin_init(ha_entity_origin_t *origin) {
    if (origin == NULL) {
        return ESP_ERR_INVALID_ARG;  // Handle NULL input
    }

    // Allocate and copy the name
    origin->name = strdup(HA_DEVICE_ORIGIN_NAME);
    if (origin->name == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for origin name");
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy the URL
    origin->url = strdup(HA_DEVICE_ORIGIN_URL);
    if (origin->url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for origin URL");
        free(origin->name);  // Clean up previous allocation
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy the software version
    origin->sw = strdup(esp_get_idf_version());
    if (origin->sw == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for origin software version");
        free(origin->name);  // Clean up previous allocations
        free(origin->url);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Origin initialized: name=%s, url=%s, sw=%s", origin->name, origin->url, origin->sw);
    return ESP_OK;
}


/**
 * @brief: Destroy the device origin entity and free up memory
 */
esp_err_t ha_origin_free(ha_entity_origin_t *origin) {
    if (origin != NULL) {
        free(origin->name);
        free(origin->url);
        free(origin->sw);
        free(origin);
    }
    return ESP_OK;
}

/**
 * @brief: Convert device entity to JSON
 */
cJSON *ha_origin_to_JSON(ha_entity_origin_t *origin) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "sw", origin->sw);
    cJSON_AddStringToObject(root, "url", origin->url);
    cJSON_AddStringToObject(root, "name", origin->name);

    return root;
}

/**
 * @brief: Initialize basic device origin entity.
 */
esp_err_t ha_entity_discovery_init(ha_entity_discovery_t *discovery) {
    if (discovery == NULL) {
        ESP_LOGE(TAG, "Got NULL as ENTITY DISCOVERY. Please, init it first!");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize availability array with 1 element
    discovery->availability = (ha_entity_availability_t *)malloc(1 * sizeof(ha_entity_availability_t));
    if (discovery->availability == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for availability");
        return ESP_ERR_NO_MEM;
    }

    // Initialize availability[0]
    esp_err_t err = ha_availability_init(&discovery->availability[0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize availability");
        free(discovery->availability);
        return err;
    }

    // Allocate and initialize the device
    discovery->device = (ha_device_t *)malloc(sizeof(ha_device_t));
    if (discovery->device == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for DEVICE");
        free(discovery->availability);
        return ESP_ERR_NO_MEM;
    }

    err = ha_device_init(discovery->device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DEVICE");
        free(discovery->availability);
        free(discovery->device);
        return err;
    }

    // Log device details
    ESP_LOGD(TAG, "DISCOVERY::DEVICE: manufacturer: %s", discovery->device->manufacturer);
    ESP_LOGD(TAG, "DISCOVERY::DEVICE: model: %s", discovery->device->model);
    ESP_LOGD(TAG, "DISCOVERY::DEVICE: name: %s", discovery->device->name);
    ESP_LOGD(TAG, "DISCOVERY::DEVICE: configuration_url: %s", discovery->device->configuration_url);
    ESP_LOGD(TAG, "DISCOVERY::DEVICE: via_device: %s", discovery->device->via_device);
    ESP_LOGD(TAG, "DISCOVERY::DEVICE: identifiers[0]: %s", discovery->device->identifiers[0]);

    // Allocate and initialize the origin
    discovery->origin = (ha_entity_origin_t *)malloc(sizeof(ha_entity_origin_t));
    if (discovery->origin == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ORIGIN");
        free(discovery->availability);
        free(discovery->device);
        return ESP_ERR_NO_MEM;
    }

    err = ha_origin_init(discovery->origin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ORIGIN");
        free(discovery->availability);
        free(discovery->device);
        free(discovery->origin);
        return err;
    }

    // Log origin details
    ESP_LOGD(TAG, "DISCOVERY::ORIGIN: name: %s", discovery->origin->name);
    ESP_LOGD(TAG, "DISCOVERY::ORIGIN: sw: %s", discovery->origin->sw);
    ESP_LOGD(TAG, "DISCOVERY::ORIGIN: url: %s", discovery->origin->url);

    // Allocate and read MQTT prefix and device ID from NVS
    char *mqtt_prefix = NULL;
    char *device_id = NULL;

    err = nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT prefix from NVS");
        free(discovery->availability);
        free(discovery->device);
        free(discovery->origin);
        return err;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID from NVS");
        free(discovery->availability);
        free(discovery->device);
        free(discovery->origin);
        return err;
    }

    // Initialize discovery fields
    discovery->enabled_by_default = true;
    size_t topic_len = strlen(mqtt_prefix) + strlen(device_id) + strlen(HA_DEVICE_STATE_PATH) + 3;  // for slashes and null terminator

    discovery->json_attributes_topic = (char *)malloc(topic_len);
    if (discovery->json_attributes_topic == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON attributes topic");
        free(discovery->availability);
        free(discovery->device);
        free(discovery->origin);
        free(mqtt_prefix);
        free(device_id);
        return ESP_ERR_NO_MEM;
    }
    snprintf(discovery->json_attributes_topic, topic_len, "%s/%s/%s", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH);

    discovery->state_topic = (char *)malloc(topic_len);
    if (discovery->state_topic == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for state topic");
        free(discovery->availability);
        free(discovery->device);
        free(discovery->origin);
        free(discovery->json_attributes_topic);
        free(mqtt_prefix);
        free(device_id);
        return ESP_ERR_NO_MEM;
    }
    snprintf(discovery->state_topic, topic_len, "%s/%s/%s", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH);

    // Cleanup temporary variables
    free(mqtt_prefix);
    free(device_id);

    return ESP_OK;
}

/**
 * @brief: Fulfills extended entity discovery for a specified metric and device class
 */
esp_err_t ha_entity_discovery_fullfill(ha_entity_discovery_t *discovery, const char* metric, const char* unit, const char* device_class, const char* state_class) {
    if (discovery == NULL || metric == NULL || unit == NULL || device_class == NULL) {
        ESP_LOGE(TAG, "Invalid argument(s) passed to ha_entity_discovery_fullfill");
        return ESP_ERR_INVALID_ARG;
    }

    // Call ha_entity_discovery_init first
    esp_err_t err = ha_entity_discovery_init(discovery);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initiate ENTITY DISCOVERY entity");
        return err;
    }

    // Allocate and read device ID and device serial
    char *device_id = NULL;
    char *device_serial = NULL;

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);  // Removed & before device_id
    if (err != ESP_OK || device_id[0] == '\0') {  // Check if the device_id was actually read
        ESP_LOGE(TAG, "Failed to read device ID from NVS or device ID is empty");
        return err;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial);  // Removed & before device_serial
    if (err != ESP_OK || device_serial[0] == '\0') {  // Check if the device_serial was actually read
        ESP_LOGE(TAG, "Failed to read device serial from NVS or device serial is empty");
        free(device_id);
        return err;
    }

    // Allocate memory for object_id and format it
    if (device_id != NULL && metric != NULL) {  // Double-check for NULL before strlen
        discovery->object_id = (char *)malloc(strlen(device_id) + strlen(metric) + 2);  // +2 for '_' and null terminator
        if (discovery->object_id == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for object_id");
            free(device_id);
            free(device_serial);
            return ESP_ERR_NO_MEM;
        }
        sprintf(discovery->object_id, "%s_%s", device_id, metric);
    } else {
        ESP_LOGE(TAG, "Invalid device_id or metric");
        free(device_id);
        free(device_serial);
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate memory for unique_id and format it
    discovery->unique_id = (char *)malloc(strlen(device_id) + strlen(device_serial) + strlen(metric) + 3);  // +3 for two '_' and null terminator
    if (discovery->unique_id == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for unique_id");
        free(device_id);
        free(device_serial);
        free(discovery->object_id);
        return ESP_ERR_NO_MEM;
    }
    sprintf(discovery->unique_id, "%s_%s_%s", device_id, device_serial, metric);

    // Set unit of measurement, device & state class
    discovery->unit_of_measurement = unit;
    discovery->device_class = device_class;
    discovery->state_class = state_class;

    // Allocate memory for value_template and format it
    const char *template_prefix = "{{ value_json.";
    const char *template_suffix = " }}";
    size_t value_template_len = strlen(template_prefix) + strlen(metric) + strlen(template_suffix) + 1;
    
    discovery->value_template = (char *)malloc(value_template_len);
    if (discovery->value_template == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for value_template");
        free(device_id);
        free(device_serial);
        free(discovery->object_id);
        free(discovery->unique_id);
        return ESP_ERR_NO_MEM;
    }
    sprintf(discovery->value_template, "%s%s%s", template_prefix, metric, template_suffix);

    // Clean up temporary variables
    free(device_id);
    free(device_serial);

    return ESP_OK;
}

/**
 * @brief: Destroy entity discovery entity and free up memory
 */
esp_err_t ha_entity_discovery_free(ha_entity_discovery_t *discovery) {

    if (discovery != NULL) {
        if (discovery->availability != NULL) {
            ha_availability_free(discovery->availability);  // Assuming a single availability entity for now
        }

        ha_device_free(discovery->device);  // Free device-related memory
        ha_origin_free(discovery->origin);  // Free origin-related memory
        

        free(discovery->json_attributes_topic);  // Free dynamically allocated strings
        free(discovery->state_topic);
        free(discovery->object_id);
        free(discovery->unique_id);
        free(discovery->value_template);

        // free(discovery);  // Finally, free the discovery struct itself
    }

    return ESP_OK;
}

/**
 * @brief: Convert entity discovery entity to JSON
 */
cJSON *ha_entity_discovery_to_JSON(ha_entity_discovery_t *discovery) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "device", ha_device_to_JSON(discovery->device));
    cJSON_AddItemToObject(root, "origin", ha_origin_to_JSON(discovery->origin));

    cJSON *availability_array = cJSON_CreateArray();
    cJSON_AddItemToArray(availability_array, ha_availability_to_JSON(&discovery->availability[0]));
    cJSON_AddItemToObject(root, "availability", availability_array);

    cJSON_AddStringToObject(root, "device_class", discovery->device_class);
    cJSON_AddBoolToObject(root, "enabled_by_default", &discovery->enabled_by_default);
    cJSON_AddStringToObject(root, "json_attributes_topic", discovery->json_attributes_topic);
    cJSON_AddStringToObject(root, "object_id", discovery->object_id);
    cJSON_AddStringToObject(root, "state_class", discovery->state_class);
    cJSON_AddStringToObject(root, "state_topic", discovery->state_topic);
    cJSON_AddStringToObject(root, "unique_id", discovery->unique_id);
    cJSON_AddStringToObject(root, "unit_of_measurement", discovery->unit_of_measurement);
    cJSON_AddStringToObject(root, "value_template", discovery->value_template);

    return root;
}

/**
 * @brief: Print JSON text of entity discovery entity
 */
char* ha_entity_discovery_print_JSON(ha_entity_discovery_t *discovery) {
    cJSON *j_entity_discovery = ha_entity_discovery_to_JSON(discovery);
    char *json = NULL;
    json = cJSON_Print(j_entity_discovery);
    cJSON_Delete(j_entity_discovery);
    return json;
}

/**
 * @brief: Serialize pressure sensor data to JSON
 *
 */
char *serialize_sensor_state(sensor_data_t *s_data) {
    char *json = NULL;
    cJSON *root = cJSON_CreateObject();

    // Debugging: Print sensor data before serializing
    /*
    ESP_LOGD(TAG, "Data for the serialization (in function): Raw ADC Value: %d, Voltage: %.3f V, Pressure: %.3f Pa", 
             s_data->voltage_raw, s_data->voltage, s_data->pressure);
    */

    cJSON *j_pressure = cJSON_CreateNumber(s_data->pressure);
    if (j_pressure != NULL) {
        cJSON_AddItemToObject(root, "pressure", j_pressure);
    }

    cJSON *j_voltage = cJSON_CreateNumber(s_data->voltage);
    if (j_voltage != NULL) {
        cJSON_AddItemToObject(root, "voltage", j_voltage);
    }

    cJSON *j_voltage_offset = cJSON_CreateNumber(s_data->voltage_offset);
    if (j_voltage_offset != NULL) {
        cJSON_AddItemToObject(root, "voltage_offset", j_voltage_offset);
    }

    cJSON *j_sensor_linear_multiplier = cJSON_CreateNumber(s_data->sensor_linear_multiplier);
    if (j_sensor_linear_multiplier != NULL) {
        cJSON_AddItemToObject(root, "sensor_linear_multiplier", j_sensor_linear_multiplier);
    }

    cJSON *j_voltage_raw = cJSON_CreateNumber(s_data->voltage_raw);
    if (j_voltage_raw != NULL) {
        cJSON_AddItemToObject(root, "voltage_raw", j_voltage_raw);
    }

    json = cJSON_Print(root);
    cJSON_Delete(root);
    return json;
}
