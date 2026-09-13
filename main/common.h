#ifndef COMMON_H
#define COMMON_H

#define _DEVICE_ENGINEERING_BUILD    false // Enable engineering build features (e.g., verbose status logging)

/**
 * Enabling functional modules
 */
#define _DEVICE_ENABLE_WIFI     true
#define _DEVICE_ENABLE_HTTP_API             (true && _DEVICE_ENABLE_WIFI)
#define _DEVICE_ENABLE_WEB                  (true && _DEVICE_ENABLE_HTTP_API)

#define _DEVICE_ENABLE_MQTT     (true && _DEVICE_ENABLE_WIFI)
#define _DEVICE_ENABLE_HA       (true && _DEVICE_ENABLE_MQTT)

#define _DEVICE_ENABLE_ZIGBEE   false   // DO NOT ENABLE both WiFi and Zigbee and do not remove dual-enablement protection. That will make device very unstable!

#define _DEVICE_ENABLE_STATUS   true
#define _DEVICE_ENABLE_STATUS_SYSINFO_MQTT          ((true && _DEVICE_ENABLE_STATUS && _DEVICE_ENABLE_MQTT) || _DEVICE_ENGINEERING_BUILD)
#define _DEVICE_ENABLE_STATUS_SYSINFO_HEAP          ((false && _DEVICE_ENABLE_STATUS) || _DEVICE_ENGINEERING_BUILD)
#define _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_CHECK    ((false && _DEVICE_ENABLE_STATUS) || _DEVICE_ENGINEERING_BUILD)
#define _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_TRACE    ((false && _DEVICE_ENABLE_STATUS) || _DEVICE_ENGINEERING_BUILD)
#define _DEVICE_ENABLE_STATUS_MEMGUARD              ((false && _DEVICE_ENABLE_STATUS) || _DEVICE_ENGINEERING_BUILD)

static const char *TAG = "PressureSensor";

#endif