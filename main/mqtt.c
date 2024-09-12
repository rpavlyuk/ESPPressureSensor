#include <stdio.h>
#include "mqtt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "non_volatile_storage.h"
#include "mqtt_client.h"

#include "settings.h"
#include "wifi.h"
#include "sensor.h"  // To access the sensor_data

esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
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
    
    // wait for Wi-Fi to connect
    while(!g_wifi_ready) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi/network to become ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    char *mqtt_server = (char *)malloc(MQTT_SERVER_LENGTH);
    char *mqtt_protocol = (char *)malloc(MQTT_PROTOCOL_LENGTH);
    char *mqtt_user = (char *)malloc(MQTT_USER_LENGTH);
    char *mqtt_password = (char *)malloc(MQTT_PASSWORD_LENGTH);
    char *mqtt_prefix = (char *)malloc(MQTT_PREFIX_LENGTH);
    char *device_id = (char *)malloc(DEVICE_ID_LENGTH+1);
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

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(mqtt_client);
}

// Function to publish sensor data
void mqtt_publish_sensor_data(const sensor_data_t *sensor_data) {

    // Ensure MQTT client is initialized
    // NOTE: Not checking for connection as CONFIG_MQTT_SKIP_PUBLISH_IF_DISCONNECTED is enabled
#if !CONFIG_MQTT_SKIP_PUBLISH_IF_DISCONNECTED
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGW(TAG, "MQTT client is not initialized or not connected. Trying to re-init...");
        if (mqtt_init() != ESP_OK) {
            ESP_LOGE(TAG, "MQTT client re-init failed. Will not publish any data to MQTT.");
            return;
        }
    }
#endif

    char *mqtt_server = (char *)malloc(MQTT_SERVER_LENGTH);
    char *mqtt_protocol = (char *)malloc(MQTT_PROTOCOL_LENGTH);
    char *mqtt_user = (char *)malloc(MQTT_USER_LENGTH);
    char *mqtt_password = (char *)malloc(MQTT_PASSWORD_LENGTH);
    char *mqtt_prefix = (char *)malloc(MQTT_PREFIX_LENGTH);
    char *device_id = (char *)malloc(DEVICE_ID_LENGTH+1);
    uint16_t mqtt_port;

    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));

    // Create MQTT topics based on mqtt_prefix and device_id
    char topic_voltage[256], topic_voltage_raw[256], topic_voltage_offset[256], topic_pressure[256], topic_multiplier[256];
    snprintf(topic_voltage, sizeof(topic_voltage), "%s/%s/voltage", mqtt_prefix, device_id);
    snprintf(topic_voltage_raw, sizeof(topic_voltage_raw), "%s/%s/voltage_raw", mqtt_prefix, device_id);
    snprintf(topic_voltage_offset, sizeof(topic_voltage_offset), "%s/%s/voltage_offset", mqtt_prefix, device_id);
    snprintf(topic_pressure, sizeof(topic_pressure), "%s/%s/pressure", mqtt_prefix, device_id);
    snprintf(topic_multiplier, sizeof(topic_multiplier), "%s/%s/multiplier", mqtt_prefix, device_id);

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

    // Publish sensor_linear_multiplier (use %lu for uint32_t)
    snprintf(value, sizeof(value), "%lu", (unsigned long)sensor_data->sensor_linear_multiplier);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_multiplier, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_multiplier);
        is_error = true;
    }

    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing sensor data to MQTT");
    } else {
        ESP_LOGI(TAG, "MQTT sensor data published successfully.");
    }
    
}