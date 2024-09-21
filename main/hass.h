#ifndef HASS_H
#define HASS_H

#include "cJSON.h"
#include "esp_system.h"

#include "common.h"
#include "sensor.h"
#include "status.h"

#define HA_DEVICE_MANUFACTURER     "espressif"
#define HA_DEVICE_MODEL            "esp32"
#define CFG_URL_LEN                26

#define HA_DEVICE_STATUS_PATH   "status"
#define HA_DEVICE_STATE_PATH    "sensor"
#define HA_DEVICE_CONFIG_PATH   "config"

#define HA_DEVICE_ORIGIN_NAME   "ESP-IDF"
#define HA_DEVICE_ORIGIN_SW     ""
#define HA_DEVICE_ORIGIN_URL    "https://github.com/espressif/esp-idf"

#define HA_DEVICE_DEVICE_CLASS  "fluid_pressure"
#define HA_DEVICE_STATE_CLASS   "measurement"
#define HA_DEVICE_FAMILY        "sensor"


typedef struct {
    char *topic;
} ha_entity_availability_t;

typedef struct {
    char *configuration_url;
    char *manufacturer;
    char *model;
    char *name;
    char *via_device;
    char *identifiers[1];
} ha_device_t;

typedef struct {
    char *name;
    char *sw;
    char *url;
} ha_entity_origin_t;

typedef struct {
    ha_entity_availability_t *availability;
    ha_device_t *device;
    char *device_class;
    bool enabled_by_default;
    char *json_attributes_topic;
    char *object_id;
    ha_entity_origin_t *origin;
    char *state_class;
    char *state_topic;
    char *unique_id;
    char *unit_of_measurement;
    char *value_template;
} ha_entity_discovery_t;

esp_err_t ha_device_init(ha_device_t *device);
esp_err_t ha_device_free(ha_device_t *device);
cJSON *ha_device_to_JSON(ha_device_t *device);
char *ha_device_to_string(ha_device_t *device);
esp_err_t ha_availability_init(ha_entity_availability_t *availability);
esp_err_t ha_availability_free(ha_entity_availability_t *availability);
cJSON *ha_availability_to_JSON(ha_entity_availability_t *availability);
esp_err_t ha_origin_init(ha_entity_origin_t *origin);
esp_err_t ha_origin_free(ha_entity_origin_t *origin);
cJSON *ha_origin_to_JSON(ha_entity_origin_t *origin);
esp_err_t ha_entity_discovery_init(ha_entity_discovery_t *discovery);
esp_err_t ha_entity_discovery_fullfill(ha_entity_discovery_t *discovery, const char* metric, const char* unit, const char* device_class, const char* state_class);
esp_err_t ha_entity_discovery_free(ha_entity_discovery_t *discovery);
cJSON *ha_entity_discovery_to_JSON(ha_entity_discovery_t *discovery);
char* ha_entity_discovery_print_JSON(ha_entity_discovery_t *discovery);

cJSON *sensor_state_to_JSON(sensor_data_t *s_data);
char *serialize_sensor_state(sensor_data_t *sensor_data);
cJSON *sensor_status_to_JSON(sensor_status_t *s_data);
char *serialize_sensor_status(sensor_status_t *s_data);
cJSON *sensor_all_to_JSON(sensor_status_t *status, sensor_data_t *sensor);
char *serialize_all_device_data(sensor_status_t *status, sensor_data_t *sensor);


#endif