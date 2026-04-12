#include "freertos/FreeRTOS.h"   // must be first
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"      // if you use queues

#include "esp_system.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include <inttypes.h>

#include "common.h"
#include "flags.h"
#include "status.h"

/**
 * @brief Resets all system event bits in the g_sys_events event group.
 * This function clears the following bits:
 * - BIT_WIFI_CONNECTED
 * - BIT_WIFI_PROVISIONED
 * - BIT_MQTT_CONNECTED
 * - BIT_MQTT_READY
 * - BIT_NVS_READY
 * - BIT_OTA_IN_PROGRESS
 * - BIT_MQTT_RELAYS_SUBSCRIBED
 * - BIT_DEVICE_READY
 * - BIT_UNITS_IN_MEMORY
 *  @return esp_err_t ESP_OK on success, ESP_FAIL if g_sys_events is not initialized.
 */
esp_err_t reset_system_bits(void) {
    if (g_sys_events == NULL) {
        ESP_LOGE(TAG, "System event group is not initialized");
        return ESP_FAIL;
    }
    xEventGroupClearBits(g_sys_events, 
        BIT_WIFI_CONNECTED | 
        BIT_WIFI_PROVISIONED | 
        BIT_MQTT_CONNECTED | 
        BIT_MQTT_READY | 
        BIT_NVS_READY | 
        BIT_OTA_IN_PROGRESS |
        BIT_MQTT_RELAYS_SUBSCRIBED |
        BIT_DEVICE_READY |
        BIT_UNITS_IN_MEMORY);
    return ESP_OK;

}

/**
 * @brief Dumps the current state of system event bits for debugging purposes.
 * 
 * This function retrieves the current bits set in the g_sys_events event group
 * and logs their states (set or not set) along with a custom message indicating
 * the context of the dump. It also logs the name of the current task for additional
 * context.
 * 
 * @param[in] why A string indicating the reason or context for dumping the bits.
 */
void dump_sys_bits(const char *why) {
    EventBits_t b = xEventGroupGetBits(g_sys_events);
    ESP_LOGI(TAG,
        "[%s] SYS bits=0x%08" PRIx32 " WIFI_CONN=%d WIFI_PROV=%d MQTT_CONN=%d MQTT_READY=%d MQTT_SUB=%d DEVICE_READY=%d UNITS_IN_MEM=%d",
        why, (uint32_t)b,
        !!(b & BIT_WIFI_CONNECTED),
        !!(b & BIT_WIFI_PROVISIONED),
        !!(b & BIT_MQTT_CONNECTED),
        !!(b & BIT_MQTT_READY),
        !!(b & BIT_MQTT_RELAYS_SUBSCRIBED),
        !!(b & BIT_DEVICE_READY),
        !!(b & BIT_UNITS_IN_MEMORY)
    );
    // Also print current task for context
    // dump_current_task();
}
