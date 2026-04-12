#ifndef MQTT_H
#define MQTT_H

#include "common.h"
#include "mqtt_client.h"
#include "sensor.h"

#define MQTT_QOS_DEFAULT    0
#define MQTT_QOS_SUBSCRIBE  1
#define MQTT_QOS_PUBLISH  MQTT_QOS_DEFAULT

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

// Define the SPIFFS configuration
#define CA_CERT_PATH "/spiffs/ca.crt"

static void log_error_if_nonzero(const char *message, int error_code);

// MQTT Event loop handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// Function to initialize the MQTT client
esp_err_t mqtt_init(void);

// Function to publish sensor data
esp_err_t mqtt_publish_sensor_data(const sensor_data_t *sensor_data);

// publish device definitions to Home Assistant
void mqtt_publish_home_assistant_config(const char *device_id, const char *mqtt_prefix, const char *homeassistant_prefix);

// HA device update task
void mqtt_device_config_task(void *param);

// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt();

#endif // MQTT_H
