#ifndef MQTT_H
#define MQTT_H

#include "common.h"
#include "mqtt_client.h"
#include "sensor.h"

typedef enum {
    MQTT_SENSOR_MODE_DISABLE,           // soft disable MQTT
    MQTT_SENSOR_MODE_NO_RECONNECT,      // connect initially to MQTT, but do NOT reconnect
    MQTT_SENSOR_MODE_AUTOCONNECT,       // connect initially to MQTT and reconnect when lost
} mqtt_connection_mode_t;

// Define the SPIFFS configuration
#define CA_CERT_PATH "/spiffs/ca.crt"

static void log_error_if_nonzero(const char *message, int error_code);
esp_err_t load_ca_certificate(char **ca_cert);
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
