#include <stdio.h>
#include "mqtt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "non_volatile_storage.h"
#include "mqtt_client.h"
#include "esp_check.h"

#include "ca_cert_manager.h"

#include "settings.h"
#include "flags.h"
#include "wifi.h"
#include "sensor.h"  // To access the sensor_data
#include "hass.h"

static bool mqtt_connected = false;

/* Queue global variables */
static QueueHandle_t mqtt_event_queue = NULL; 
static QueueHandle_t mqtt_command_queue = NULL;

/* MQTT client global variables */
esp_mqtt_client_handle_t mqtt_client = NULL;

/**
 * @brief Starts the MQTT event queue task.
 * 
 * This function creates the MQTT event queue and starts the task responsible for 
 * handling MQTT publishing requests. The task monitors the queue for events and 
 * processes them by loading the necessary sensor data from the event and publishing the 
 * state to MQTT.
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL if the queue or task creation fails
 */
esp_err_t start_mqtt_queue_task(void) {

    // Create the event queue
    mqtt_event_queue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(sensor_event_t));
    if (mqtt_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue for MQTT sensor reading publishing. Requested size: %d", sizeof(sensor_event_t)*MQTT_QUEUE_LENGTH);
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "MQTT event queue for sensor reading publishing created successfully. Queue length: %d, item size: %d", MQTT_QUEUE_LENGTH, sizeof(sensor_event_t));
    }
    // Start the MQTT event task
    xTaskCreate(mqtt_event_task, "mqtt_event_task", 8192, NULL, 5, NULL);

    return ESP_OK;
}

/**
 * @brief FreeRTOS task to handle MQTT sensor publish events.
 * 
 * This task monitors an event queue for sensor publish requests. When an event is 
 * received, it loads the sensor data from the event and then publishes the state to MQTT using the 
 * mqtt_publish_sensor_data() function.
 * 
 * @param[in] arg Unused task argument.
 */
void mqtt_event_task(void *arg) {
    sensor_event_t event;
    sensor_data_t sensor_data;
    esp_err_t err;

    while (1) {
        // Wait for events to arrive in the queue
        if (xQueueReceive(mqtt_event_queue, &event, portMAX_DELAY)) {
            sensor_data = event.sensor_data;
            ESP_LOGI(TAG, "mqtt_event_task: Recevied MQTT publish message. Sensor data: %.2f Pa", sensor_data.pressure);

            // Publish sensor state to MQTT
            err = mqtt_publish_sensor_data(&sensor_data);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to publish sensor data to MQTT: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "Sensor data published to MQTT successfully");    
            }
        }
    }
}

/**
 * @brief Sends a sensor data publish event to the MQTT queue.
 * 
 * This function triggers an MQTT publish by sending a sensor event to the 
 * mqtt_event_queue. The event includes the sensor data to be published.
 * The event will be processed by the MQTT event task, which will publish the
 * sensor data to MQTT.
 * 
 * @param[in] sensor_data The sensor data to be published.
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL if the event cannot be sent to the queue
 */
esp_err_t  trigger_mqtt_publish(const sensor_data_t *sensor_data) {
    sensor_event_t event;

    event.sensor_data = *sensor_data;

    ESP_LOGI(TAG, "%s: +-> Pushing MQTT publish event to the queue. Sensor data: %.2f Pa", __func__, event.sensor_data.pressure);

    // Send the event to the MQTT event task
    if (xQueueSend(mqtt_event_queue, &event, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send event to MQTT queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Logs an error message if the provided error code is non-zero.
 * 
 * This function checks the provided error code and logs an error message if the code is non-zero. It is used to log detailed error information for MQTT connection errors, especially those related to TLS/SSL
 * errors. The function is typically called when handling MQTT events to provide more context about the nature of the error.
 * @param[in] message A descriptive message to include in the log.
 * @param[in] error_code The error code to check and log if non-zero.
 * @return None
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
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
        mqtt_connected = true;  // Set flag when connected
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(g_sys_events, BIT_MQTT_CONNECTED);
        xEventGroupSetBits(g_sys_events, BIT_MQTT_READY);
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;  // Reset flag when disconnected
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED);
        xEventGroupClearBits(g_sys_events, BIT_MQTT_READY);
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
        xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED | BIT_MQTT_READY);
        mqtt_connected = false;  // Handle connection error
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * @brief Initialize the MQTT client.
 * 
 * This function initializes the MQTT client with the configuration parameters
 * stored in NVS. The function reads the MQTT server, port, protocol, username,
 * password, and prefix from NVS and uses them to configure the MQTT client.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the MQTT client cannot be initialized.
 * 
 */
esp_err_t mqtt_init(void) {
    
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK; // not an issue
    }

    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,
        BIT_WIFI_CONNECTED,
        pdFALSE,                // don't clear
        pdTRUE,
        pdMS_TO_TICKS(30000)    // wait up to 30 seconds
    );

    if (bits & BIT_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi/network is ready!");
    } else {
        ESP_LOGW(TAG, "Timeout waiting for Wi-Fi to connect");
        return ESP_FAIL;
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
        if (load_ca_certificate(&ca_cert, CA_CERT_PATH_MQTTS) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load CA certificate");
            if (strcmp(mqtt_protocol,"mqtts") == 0) {
                ESP_LOGE(TAG, "MQTTS protocol cannot be managed without CA certificate.");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGI(TAG, "Loaded CA certificate: %s", CA_CERT_PATH_MQTTS);
        }
        if (ca_cert) {
            mqtt_cfg.broker.verification.certificate = ca_cert;
        }
    }

    esp_err_t ret;
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ret = esp_mqtt_client_start(mqtt_client);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(device_id);

    // 🟢 Wait up to 10 seconds for MQTT to become fully ready
    ESP_LOGI(TAG, "Waiting for MQTT client to connect...");

    bits = xEventGroupWaitBits(
        g_sys_events,
        BIT_MQTT_CONNECTED | BIT_MQTT_READY,  // wait for both
        pdFALSE,                              // don’t clear bits
        pdTRUE,                               // wait for *all* bits
        pdMS_TO_TICKS(10000)                  // timeout 10 seconds
    );

    if ((bits & (BIT_MQTT_CONNECTED | BIT_MQTT_READY)) ==
        (BIT_MQTT_CONNECTED | BIT_MQTT_READY)) {
        ESP_LOGI(TAG, "MQTT is connected and ready!");
    } else {
        ESP_LOGE(TAG, "Timeout waiting for MQTT to connect/initialize");
        ret = ESP_FAIL;
    }

    return ret;
}

/**
 * @brief Publishes sensor data to MQTT topics.
 * 
 * This function publishes the provided sensor data to MQTT topics based on the configured MQTT prefix and device ID. It checks if MQTT is enabled in the settings and waits for the
 * MQTT connection to become ready before attempting to publish. Each field of the sensor data is published to a separate MQTT topic.
 * @param[in] sensor_data The sensor data to be published.
 * @return ESP_OK if the data was published successfully, or an error code if MQTT is disabled or if the MQTT connection is not ready within the timeout period.
 * 
 */
esp_err_t mqtt_publish_sensor_data(const sensor_data_t *sensor_data) {

    uint16_t mqtt_connection_mode;
    esp_err_t err;

    // Read MQTT connection mode
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Failed to read MQTT connection mode from NVS", __func__);
        return ESP_FAIL;
    }

    // Check if MQTT is disabled in the device settings
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "%s: MQTT disabled in device settings. Publishing skipped.", __func__);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "%s: Waiting for MQTT connection to become ready...", __func__);

    // Wait up to 10 seconds total
    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,             // event group handle
        BIT_MQTT_CONNECTED | BIT_MQTT_READY,       // bit(s) to wait for
        pdFALSE,                  // don't clear the bit on exit
        pdTRUE,                   // wait for all bits (only one here)
        pdMS_TO_TICKS(10000)      // timeout 10 seconds
    );

    if ((bits & BIT_MQTT_CONNECTED) && (bits & BIT_MQTT_READY)) {
        ESP_LOGD(TAG, "%s: MQTT connection is ready!", __func__);
        // Continue normal operation
    } else {
        ESP_LOGE(TAG, "%s: MQTT never became ready after 10 seconds", __func__);
        return ESP_FAIL;
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
    char topic_voltage[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN], \
        topic_voltage_raw[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN], \
        topic_voltage_offset[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN], \
        topic_pressure[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN], \
        topic_multiplier[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN], \
        topic_state[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN];
    snprintf(topic_voltage, sizeof(topic_voltage), "%s/%s/%s/voltage", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(topic_voltage_raw, sizeof(topic_voltage_raw), "%s/%s/%s/voltage_raw", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(topic_voltage_offset, sizeof(topic_voltage_offset), "%s/%s/%s/voltage_offset", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(topic_pressure, sizeof(topic_pressure), "%s/%s/%s/pressure", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(topic_multiplier, sizeof(topic_multiplier), "%s/%s/%s/multiplier", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(topic_state, sizeof(topic_state), "%s/%s/%s", mqtt_prefix, device_id, HA_DEVICE_STATE_PATH_SENSOR);

    // Publish each sensor data field separately
    int msg_id;
    bool is_error = false;
    char value[SENSOR_VALUE_STRING_MAX_LEN];

    // Publish voltage
    snprintf(value, sizeof(value), "%.3f", sensor_data->voltage);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_voltage, value, 0, MQTT_QOS_PUBLISH, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_voltage);
        is_error = true;
    }

    // Publish voltage_raw
    snprintf(value, sizeof(value), "%d", sensor_data->voltage_raw);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_voltage_raw, value, 0, MQTT_QOS_PUBLISH, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_voltage_raw);
        is_error = true;
    }

    // Publish voltage_offset
    snprintf(value, sizeof(value), "%.3f", sensor_data->voltage_offset);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_voltage_offset, value, 0, MQTT_QOS_PUBLISH, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_voltage_offset);
        is_error = true;
    }

    // Publish pressure
    snprintf(value, sizeof(value), "%.2f", sensor_data->pressure);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_pressure, value, 0, MQTT_QOS_PUBLISH, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic_pressure);
        is_error = true;
    }

    // Publish sensor_linear_multiplier
    snprintf(value, sizeof(value), "%lu", (unsigned long)sensor_data->sensor_linear_multiplier);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic_multiplier, value, 0, MQTT_QOS_PUBLISH, 0);
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
        msg_id = esp_mqtt_client_publish(mqtt_client, topic_state, sensor_data_json, 0, MQTT_QOS_PUBLISH, true);
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

/**
 * @brief Cleans up the MQTT client resources.
 */
void cleanup_mqtt() {
    if (mqtt_client) {
        mqtt_connected = false;
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_stop(mqtt_client), TAG, "Failed to stop the MQTT client");
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_destroy(mqtt_client), TAG, "Failed to destroy the MQTT client");  // Free the resources
        mqtt_client = NULL;
    }
}

/**
 * @brief Publish Home Assistant discovery configuration for the device and its sensors
 * 
 * This function publishes the Home Assistant discovery configuration for the device and its sensors. It waits for the MQTT connection to become ready, checks if MQTT is enabled in the device settings, and then constructs and publishes the discovery messages for each sensor metric (pressure, voltage, voltage offset) using the Home Assistant MQTT Discovery format.
 * @param device_id The unique identifier for the device (e.g., MAC address or serial number)
 * @param mqtt_prefix The MQTT topic prefix to use for publishing
 * @param homeassistant_prefix The MQTT topic prefix for Home Assistant discovery (e.g., "homeassistant")
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t mqtt_publish_home_assistant_config(const char *device_id, const char *mqtt_prefix, const char *homeassistant_prefix) {

    // Wait up to 10 seconds total
    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,             // event group handle
        BIT_MQTT_CONNECTED | BIT_MQTT_READY,       // bit(s) to wait for
        pdFALSE,                  // don't clear the bit on exit
        pdTRUE,                   // wait for all bits (only one here)
        pdMS_TO_TICKS(10000)      // timeout 10 seconds
    );

    if ((bits & BIT_MQTT_CONNECTED) && (bits & BIT_MQTT_READY)) {
        ESP_LOGI(TAG, "%s: MQTT connection is ready!", __func__);
        // Continue normal operation
    } else {
        ESP_LOGE(TAG, "%s: MQTT never became ready after 10 seconds", __func__);
        return ESP_FAIL;
    }

    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "%s: MQTT disabled in device settings. Publishing skipped.", __func__);
        return ESP_FAIL;
    }
    
    char topic[MQTT_TOPIC_HA_INTEGRATION_MAX_LEN];
    char payload[MQTT_PAYLOAD_HA_INTEGRATION_MAX_LEN];
    char discovery_path[MQTT_TOPIC_SENSOR_VALUE_MAX_LEN];
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
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "%s: Entity discovery for %s initialized successfully", __func__, metric);
        ha_entity_discovery_print(entity_discovery);  // Print the discovery struct for debugging
    }

    char *discovery_json = ha_entity_discovery_print_JSON(entity_discovery);
    ESP_LOGI(TAG, "%s: Device discovery serialized:\n%s", __func__, discovery_json);
    memset(discovery_path, 0, sizeof(discovery_path));
    sprintf(discovery_path, "%s/%s", homeassistant_prefix, HA_DEVICE_FAMILY);
    sprintf(topic, "%s/%s/%s/%s", discovery_path, device_id, metric, HA_DEVICE_CONFIG_PATH);

    msg_id = esp_mqtt_client_publish(mqtt_client, topic, discovery_json, 0, MQTT_QOS_PUBLISH, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Discovery topic %s not published", topic);
        is_error = true;
    }
    // tell we are online
    // Publish availability as "online"
    char *ha_availability_entry_json = ha_availability_entry_print_JSON("online");
    msg_id = esp_mqtt_client_publish(
        mqtt_client,
        entity_discovery->availability->topic,
        ha_availability_entry_json,
        0, MQTT_QOS_PUBLISH, 1);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Availability topic %s not published",
                entity_discovery->availability->topic);
        is_error = true;
    }
    ha_entity_discovery_free(entity_discovery);

    /* Voltage */
    unit = "V";
    metric = "voltage";
    device_class = "voltage";
    state_class = "measurement";
    if (ha_entity_discovery_fullfill(entity_discovery, metric, unit, device_class, state_class) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to initiate entity discovery for %s", metric);
        return ESP_FAIL;
    }

    discovery_json = ha_entity_discovery_print_JSON(entity_discovery);
    ESP_LOGI(TAG, "Device discovery serialized:\n%s", discovery_json);
    memset(discovery_path, 0, sizeof(discovery_path));
    sprintf(discovery_path, "%s/%s", homeassistant_prefix, HA_DEVICE_FAMILY);
    sprintf(topic, "%s/%s/%s/%s", discovery_path, device_id, metric, HA_DEVICE_CONFIG_PATH);

    msg_id = esp_mqtt_client_publish(mqtt_client, topic, discovery_json, 0, MQTT_QOS_PUBLISH, 0);
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
        return ESP_FAIL;
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
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Home Assistant device configuration published.");
        return ESP_OK;
    }
}

/**
 * @brief Task to periodically publish Home Assistant discovery configuration for the device and its sensors
 * 
 * This FreeRTOS task periodically publishes the Home Assistant discovery configuration for the device and its sensors. It reads the MQTT prefix, device ID, Home Assistant prefix, and update interval from NVS, and then enters a loop where it calls
 * mqtt_publish_home_assistant_config() to publish the discovery configuration. The task waits for the defined update interval before publishing the configuration again, allowing Home Assistant to stay updated with any changes in the device configuration or sensor metrics.
 * @param param Unused task parameter.
 * @return None
 */
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

/**
 * @brief Stop the MQTT client.
 * 
 * This function stops the MQTT client and frees the resources used by the client.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the client cannot be stopped.
 */
esp_err_t mqtt_stop(void) {
    if (mqtt_client) {
        cleanup_mqtt();
    }
    return ESP_OK;
}

/**
 * @brief: Validate MQTT connection mode value
 * 
 * This function validates the MQTT connection mode value. The function checks if the
 * value is within the valid range of MQTT connection modes.
 * 
 * @param[in] v The MQTT connection mode value to validate.
 * 
 * @return bool   true if the value is valid, false otherwise.
 */
bool mqtt_conn_mode_is_valid(int v)
{
    return (v >= MQTT_CONN_MODE_DISABLE) && (v <= MQTT_CONN_MODE_AUTOCONNECT);
}

