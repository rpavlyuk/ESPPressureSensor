#include <stdio.h>
#include "mqtt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "non_volatile_storage.h"
#include "mqtt_client.h"
#include "esp_check.h"

#include "settings.h"
#include "wifi.h"
#include "sensor.h"  // To access the sensor_data
#include "hass.h"

esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

esp_err_t load_ca_certificate(char **ca_cert)
{
    FILE *f = fopen(CA_CERT_PATH, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open CA certificate file");
        return ESP_FAIL;
    }

    // Seek to the end to find the file size
    fseek(f, 0, SEEK_END);
    long cert_size = ftell(f);
    fseek(f, 0, SEEK_SET);  // Go back to the beginning of the file

    if (cert_size <= 0) {
        ESP_LOGE(TAG, "Invalid CA certificate file size");
        fclose(f);
        return ESP_FAIL;
    }

    // Allocate memory for the certificate
    *ca_cert = (char *) malloc(cert_size + 1);  // +1 for the null terminator
    if (*ca_cert == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for CA certificate");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    // Read the certificate into the buffer
    fread(*ca_cert, 1, cert_size, f);
    (*ca_cert)[cert_size] = '\0';  // Null-terminate the string

    fclose(f);
    ESP_LOGI(TAG, "Successfully loaded CA certificate");

    return ESP_OK;
}

esp_err_t save_ca_certificate(const char *ca_cert)
{
    if (ca_cert == NULL) {
        ESP_LOGE(TAG, "CA certificate data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(CA_CERT_PATH, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open CA certificate file for writing");
        return ESP_FAIL;
    }

    // Write the certificate to the file
    size_t written = fwrite(ca_cert, 1, strlen(ca_cert), f);
    if (written != strlen(ca_cert)) {
        ESP_LOGE(TAG, "Failed to write entire CA certificate to file");
        fclose(f);
        return ESP_FAIL;
    }

    fclose(f);
    ESP_LOGI(TAG, "Successfully saved CA certificate");

    return ESP_OK;
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    /*
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    */
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;  // Set flag when connected
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;  // Reset flag when disconnected
        cleanup_mqtt();  // Ensure proper cleanup on disconnection
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        mqtt_connected = false;  // Handle connection error
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

// Function to initialize the MQTT client
esp_err_t mqtt_init(void) {
    
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_SENSOR_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK; // not an issue
    }

    // wait for Wi-Fi to connect
    while(!g_wifi_ready) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi/network to become ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Proceed with MQTT connection
    char *mqtt_server = NULL;
    char *mqtt_protocol = NULL;
    char *mqtt_user = NULL;
    char *mqtt_password = NULL;
    char *mqtt_prefix = NULL;
    char *device_id = NULL;

    uint16_t mqtt_port;

    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));

    char broker_url[256];
    snprintf(broker_url, sizeof(broker_url), "%s://%s:%d", mqtt_protocol, mqtt_server, mqtt_port);
    ESP_LOGI(TAG, "MQTT Broker URL: %s", broker_url);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
        .network.timeout_ms = 5000,  // Increase timeout if needed
    };

    if (mqtt_user[0]) {
        mqtt_cfg.credentials.username = mqtt_user;
    }
    if (mqtt_password[0]) {
        mqtt_cfg.credentials.authentication.password = mqtt_password;
    }
    if (strcmp(mqtt_protocol,"mqtts") == 0) {
        // Load the CA certificate
        char *ca_cert = NULL;
        if (load_ca_certificate(&ca_cert) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load CA certificate");
            if (strcmp(mqtt_protocol,"mqtts") == 0) {
                ESP_LOGE(TAG, "MQTTS protocol cannot be managed without CA certificate.");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGI(TAG, "Loaded CA certificate: %s", CA_CERT_PATH);
        }
        if (ca_cert) {
            mqtt_cfg.broker.verification.certificate = ca_cert;
        }
    }

    esp_err_t ret;
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ret = esp_mqtt_client_start(mqtt_client);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(device_id);

    return ret;
}

// Function to publish sensor data
esp_err_t mqtt_publish_sensor_data(const sensor_data_t *sensor_data) {

    uint16_t mqtt_connection_mode;
    esp_err_t err;

    // Read MQTT connection mode
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT connection mode from NVS");
        return ESP_FAIL;
    }

    // Check if MQTT is disabled in the device settings
    if (mqtt_connection_mode < (uint16_t)MQTT_SENSOR_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK;
    }

    // Ensure MQTT client is initialized and connected
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGW(TAG, "MQTT client is not initialized or not connected.");
        if (mqtt_connection_mode > (uint16_t)MQTT_SENSOR_MODE_NO_RECONNECT) {
            ESP_LOGI(TAG, "Restoring connection to MQTT...");
            if (mqtt_init() != ESP_OK) {
                ESP_LOGE(TAG, "MQTT client re-init failed. Will not publish any data to MQTT.");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGW(TAG, "Re-connect disabled by MQTT mode setting. Visit device WEB interface to adjust it.");
            return ESP_FAIL;
        }
    }

    /* Declare NULL pointer for string variables */
    char *mqtt_prefix = NULL;
    char *device_id = NULL;

    // Read MQTT prefix and device ID from NVS
    err = nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT prefix from NVS");
        return ESP_ERR_NVS_BASE;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID from NVS");
        return ESP_ERR_NVS_BASE;
    }

    // Create MQTT topics based on mqtt_prefix and device_id
    char topic_voltage[256], topic_voltage_raw[256], topic_voltage_offset[256], topic_pressure[256], topic_multiplier[256], topic_state[256];
    snprintf(topic_voltage, sizeof(topic_voltage), "%s/%s/sensor/voltage", mqtt_prefix, device_id);
    snprintf(topic_voltage_raw, sizeof(topic_voltage_raw), "%s/%s/sensor/voltage_raw", mqtt_prefix, device_id);
    snprintf(topic_voltage_offset, sizeof(topic_voltage_offset), "%s/%s/sensor/voltage_offset", mqtt_prefix, device_id);
    snprintf(topic_pressure, sizeof(topic_pressure), "%s/%s/sensor/pressure", mqtt_prefix, device_id);
    snprintf(topic_multiplier, sizeof(topic_multiplier), "%s/%s/sensor/multiplier", mqtt_prefix, device_id);
    snprintf(topic_state, sizeof(topic_state), "%s/%s/sensor", mqtt_prefix, device_id);

    // Publish each sensor data field separately
    int msg_id;
    bool is_error = false;
    char value[32];

    // Publish voltage
    snprintf(value, sizeof(value), "%.3f", sensor_data->voltage);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_voltage, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_voltage);
        is_error = true;
    }

    // Publish voltage_raw
    snprintf(value, sizeof(value), "%d", sensor_data->voltage_raw);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_voltage_raw, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_voltage_raw);
        is_error = true;
    }

    // Publish voltage_offset
    snprintf(value, sizeof(value), "%.3f", sensor_data->voltage_offset);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_voltage_offset, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_voltage_offset);
        is_error = true;
    }

    // Publish pressure
    snprintf(value, sizeof(value), "%.2f", sensor_data->pressure);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_pressure, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_pressure);
        is_error = true;
    }

    // Publish sensor_linear_multiplier
    snprintf(value, sizeof(value), "%lu", (unsigned long)sensor_data->sensor_linear_multiplier);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_multiplier, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_multiplier);
        is_error = true;
    }

    // Free allocated resources for MQTT prefix and device ID
    free(mqtt_prefix);
    free(device_id);

    // Publishing JSON data
    sensor_data_t s_data = get_sensor_data();  // Create a copy of sensor_data to ensure consistency
    char *sensor_data_json = serialize_sensor_state(&s_data);
    if (sensor_data_json != NULL) {
        ESP_LOGI(TAG, "Sensor data serialized:\n%s", sensor_data_json);
        msg_id = esp_mqtt_client_publish(mqtt_client, topic_state, sensor_data_json, 0, 0, true);
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Topic %s not published", topic_state);
            is_error = true;
        }
        free(sensor_data_json);  // Free the JSON string after use
    } else {
        ESP_LOGE(TAG, "Failed to serialize sensor data");
        is_error = true;
    }

    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing sensor data to MQTT");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "MQTT sensor data published successfully.");
        return ESP_OK;
    }
}

// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt() {
    if (mqtt_client) {
        mqtt_connected = false;
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_stop(mqtt_client), TAG, "Failed to stop the MQTT client");
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_destroy(mqtt_client), TAG, "Failed to destroy the MQTT client");  // Free the resources
        mqtt_client = NULL;
    }
}

void mqtt_publish_home_assistant_config(const char *device_id, const char *mqtt_prefix, const char *homeassistant_prefix) {
    
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_SENSOR_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return;
    }

    // wait for MQTT to connect
    int i = 0, c_limit = 10;
    while(!mqtt_connected) {
        ESP_LOGI(TAG, "Waiting for MQTT connection to become ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (++i > c_limit) {
            ESP_LOGE(TAG, "MQTT never became ready after %i seconds", c_limit);
            return;
        }
    }
    
    char topic[512];
    char payload[512];
    char discovery_path[256];
    int msg_id;
    bool is_error = false;
    char *metric;
    char *unit;
    char *device_class;
    char *state_class;

    // serialized device information
    /* Pressure */
    ha_entity_discovery_t *entity_discovery = (ha_entity_discovery_t *)malloc(sizeof(ha_entity_discovery_t));
    unit = "Pa";
    metric = "pressure";
    device_class = "pressure";
    state_class = "measurement";
    if (ha_entity_discovery_fullfill(entity_discovery, metric, unit, device_class, state_class) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initiate entity discovery for %s", metric);
        return;
    }

    char *discovery_json = ha_entity_discovery_print_JSON(entity_discovery);
    ESP_LOGI(TAG, "Device discovery serialized:\n%s", discovery_json);
    memset(discovery_path, 0, sizeof(discovery_path));
    sprintf(discovery_path, "%s/%s", homeassistant_prefix, HA_DEVICE_FAMILY);
    sprintf(topic, "%s/%s/%s/%s", discovery_path, device_id, metric, HA_DEVICE_CONFIG_PATH);

    msg_id = esp_mqtt_client_publish(mqtt_client, topic, discovery_json, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Discovery topic %s not published", topic);
        is_error = true;
    }
    // tell we are online
    msg_id = esp_mqtt_client_publish(mqtt_client, entity_discovery->availability->topic, "online", 0, 0, true);

    ha_entity_discovery_free(entity_discovery);

    /* Voltage */
    unit = "V";
    metric = "voltage";
    device_class = "voltage";
    state_class = "measurement";
    if (ha_entity_discovery_fullfill(entity_discovery, metric, unit, device_class, state_class) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initiate entity discovery for %s", metric);
        return;
    }

    discovery_json = ha_entity_discovery_print_JSON(entity_discovery);
    ESP_LOGI(TAG, "Device discovery serialized:\n%s", discovery_json);
    memset(discovery_path, 0, sizeof(discovery_path));
    sprintf(discovery_path, "%s/%s", homeassistant_prefix, HA_DEVICE_FAMILY);
    sprintf(topic, "%s/%s/%s/%s", discovery_path, device_id, metric, HA_DEVICE_CONFIG_PATH);

    msg_id = esp_mqtt_client_publish(mqtt_client, topic, discovery_json, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Discovery topic %s not published", topic);
        is_error = true;
    }
    ha_entity_discovery_free(entity_discovery);

    /* Voltage Offset */
    unit = "V";
    metric = "voltage_offset";
    device_class = "voltage";
    state_class = "measurement";
    if (ha_entity_discovery_fullfill(entity_discovery, metric, unit, device_class, state_class) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initiate entity discovery for %s", metric);
        return;
    }

    discovery_json = ha_entity_discovery_print_JSON(entity_discovery);
    ESP_LOGI(TAG, "Device discovery serialized:\n%s", discovery_json);
    memset(discovery_path, 0, sizeof(discovery_path));
    sprintf(discovery_path, "%s/%s", homeassistant_prefix, HA_DEVICE_FAMILY);
    sprintf(topic, "%s/%s/%s/%s", discovery_path, device_id, metric, HA_DEVICE_CONFIG_PATH);

    msg_id = esp_mqtt_client_publish(mqtt_client, topic, discovery_json, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Discovery topic %s not published", topic);
        is_error = true;
    }
    ha_entity_discovery_free(entity_discovery);


    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing Home Assistant device configuration to MQTT.");
    } else {
        ESP_LOGI(TAG, "Home Assistant device configuration published.");
    }
}

void mqtt_device_config_task(void *param) {
    char *device_id = NULL;
    char *mqtt_prefix = NULL;
    char *ha_prefix = NULL;
    uint32_t ha_upd_intervl;

    const char* LOG_TAG = "HA MQTT DEVICE";
      
    // Load MQTT prefix from NVS
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix));

    ESP_LOGI(LOG_TAG, "Starting HA MQTT device update task. Update interval: %lu minutes.", (uint32_t) ha_upd_intervl / 1000 / 60);

    while (true) {
        // Update Home Assistant device configuration
        ESP_LOGI(LOG_TAG, "Updating HA device configurations");
        mqtt_publish_home_assistant_config(device_id, mqtt_prefix, ha_prefix);
        ESP_LOGI(LOG_TAG, "HA device configurations update complete");

        // Wait for the defined interval before the next update
        vTaskDelay(ha_upd_intervl);
    }

    free(device_id);
    free(mqtt_prefix);
    free(ha_prefix);
}

