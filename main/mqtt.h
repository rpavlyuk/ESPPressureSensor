#ifndef MQTT_H
#define MQTT_H

#include "common.h"
#include "mqtt_client.h"
#include "sensor.h"

static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// Function to initialize the MQTT client
esp_err_t mqtt_init(void);

// Function to publish sensor data
void mqtt_publish_sensor_data(const sensor_data_t *sensor_data);

#endif // MQTT_H
