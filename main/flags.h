/**
 * @file flags.h
 * @brief System event flags management
 * @author Roman Pavlyuk <roman.pavlyuk@gmail.com>
 */
#ifndef FLAGS_H
#define FLAGS_H
#include "freertos/FreeRTOS.h"

#include "freertos/event_groups.h"

extern EventGroupHandle_t g_sys_events;

// Bits (use 0..23 per FreeRTOS docs; top bits reserved)
#define BIT_WIFI_CONNECTED          (1 << 0)
#define BIT_WIFI_PROVISIONED        (1 << 1)
#define BIT_MQTT_CONNECTED          (1 << 2)
#define BIT_MQTT_READY              (1 << 3)
#define BIT_MQTT_RELAYS_SUBSCRIBED  (1 << 4)
#define BIT_NVS_READY               (1 << 5)
#define BIT_OTA_IN_PROGRESS         (1 << 6)
#define BIT_DEVICE_READY            (1 << 7)
#define BIT_UNITS_IN_MEMORY         (1 << 8)


/* Function Prototypes */
esp_err_t reset_system_bits(void);
void dump_sys_bits(const char *why);

#endif // FLAGS_H