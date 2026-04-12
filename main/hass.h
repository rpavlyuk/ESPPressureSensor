#ifndef HASS_H
#define HASS_H

#include "cJSON.h"
#include "esp_system.h"

#include "common.h"
#include "sensor.h"
#include "status.h"

#define HA_DEVICE_MANUFACTURER     "Roman Pavlyuk"
#define HA_DEVICE_MODEL            "ESP Pressure Sensor"
#define CFG_URL_LEN                26

#define HA_DEVICE_STATUS_PATH           "status"
#define HA_DEVICE_STATE_PATH_RELAY      "switch"
#define HA_DEVICE_STATE_PATH_SENSOR     "sensor"
#define HA_DEVICE_CONFIG_PATH           "config"

#define HA_DEVICE_ORIGIN_NAME   "ESP-IDF"
#define HA_DEVICE_ORIGIN_SW     ""
#define HA_DEVICE_ORIGIN_URL    "https://github.com/espressif/esp-idf"

#define HA_DEVICE_DEVICE_CLASS        "pressure"
#define HA_DEVICE_STATE_CLASS         "measurement"

#define HA_DEVICE_FAMILY              "sensor"
#define HA_DEVICE_METRIC_STATE        "state"

#define HA_DEVICE_PAYLOAD_ON          true
#define HA_DEVICE_PAYLOAD_OFF         false

#define HA_DEVICE_AVAILABILITY_VAL_TPL  "{{ value_json.state }}"


typedef struct {
    char *topic;
    char *value_template;
} ha_entity_availability_t;

typedef struct {
    char *configuration_url;
    char *manufacturer;
    char *model;
    char *name;
    char *via_device;
    char *sw_version;
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
    char *name;
    char *device_class;
    bool enabled_by_default;
    char *json_attributes_topic;
    char *object_id;
    ha_entity_origin_t *origin;
    char *state_topic;
    char *state_class;
    char *unique_id;
    char *unit_of_measurement;
    char *value_template;
    bool optimistic;
    bool payload_off;
    bool payload_on;
    char *command_topic;
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
cJSON *ha_availability_entry_to_JSON(const char *state);
char* ha_availability_entry_print_JSON(const char *state);

#endif