#ifndef COMMON_H
#define COMMON_H

/**
 * Enabling functional modules
 */
#define _DEVICE_ENABLE_WIFI     true
#define _DEVICE_ENABLE_WEB      (true && _DEVICE_ENABLE_WIFI)
#define _DEVICE_ENABLE_MQTT     (true && _DEVICE_ENABLE_WIFI)
#define _DEVICE_ENABLE_HA       (true && _DEVICE_ENABLE_MQTT)
#define _DEVICE_ENABLE_ZIGBEE   false   // DO NOT ENABLE both WiFi and Zigbee. That will make device very unstable!

static const char *TAG = "PressureSensor";

#endif