#ifndef MQTT_H
#define MQTT_H

#include "common.h"
#include "mqtt_client.h"
#include "sensor.h"

#define MQTT_QOS_DEFAULT    0
#define MQTT_QOS_SUBSCRIBE  1
#define MQTT_QOS_PUBLISH  MQTT_QOS_DEFAULT
#define MQTT_TOPIC_SENSOR_VALUE_MAX_LEN 128
#define MQTT_TOPIC_HA_INTEGRATION_MAX_LEN 512
#define MQTT_PAYLOAD_HA_INTEGRATION_MAX_LEN 512

/* Macro to check if MQTT is connected */
#define IS_MQTT_CONNECTED() \
    ((xEventGroupGetBits(g_sys_events) & BIT_MQTT_CONNECTED) != 0)

#define IS_MQTT_READY() \
    ((xEventGroupGetBits(g_sys_events) & (BIT_MQTT_CONNECTED | BIT_MQTT_READY)) == \
     (BIT_MQTT_CONNECTED | BIT_MQTT_READY))

/**
 * @brief: MQTT connection mode for the device
 */
typedef enum {
    MQTT_CONN_MODE_DISABLE = 0,           // soft disable MQTT
    MQTT_CONN_MODE_NO_RECONNECT,      // connect initially to MQTT, but do NOT reconnect
    MQTT_CONN_MODE_AUTOCONNECT,       // connect initially to MQTT and reconnect when lost
} mqtt_connection_mode_t;

/**
 * @brief: Event data used to communicate between MQTT publishing event queue and other tasks
 */
typedef struct {
    sensor_data_t sensor_data;  // The sensor data to be published
} sensor_event_t;

#define MQTT_QUEUE_LENGTH 16  // Number of items the queue can hold

esp_err_t start_mqtt_queue_task(void);
void mqtt_event_task(void *arg);
esp_err_t trigger_mqtt_publish(const sensor_data_t *sensor_data);

static void log_error_if_nonzero(const char *message, int error_code);

// MQTT Event loop handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// Function to initialize the MQTT client
esp_err_t mqtt_init(void);

// Function to publish sensor data
esp_err_t mqtt_publish_sensor_data(const sensor_data_t *sensor_data);

// publish device definitions to Home Assistant
esp_err_t mqtt_publish_home_assistant_config(const char *device_id, const char *mqtt_prefix, const char *homeassistant_prefix);

// HA device update task
void mqtt_device_config_task(void *param);

// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt();

// Call this function to stop the MQTT client (e.g., before rebooting)
esp_err_t mqtt_stop(void);

// Validate MQTT connection mode value
bool mqtt_conn_mode_is_valid(int v);

#endif // MQTT_H
