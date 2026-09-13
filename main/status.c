#include <stddef.h>
#include <stdint.h>

#include "status.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"
#include "esp_debug_helpers.h"  // For esp_backtrace_print
#include "esp_timer.h"

#include "mqtt.h"

static heap_trace_record_t trace_buffer[NUM_RECORDS];  // Buffer to store the trace records

void status_task(void *pvParameters) {
    ESP_LOGI(STATUS_TAG, "Starting system status monitoring task");
    int period = 3;
    int cycle = 0;
#if _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_TRACE
    int heap_trace_cycle = 0;
    const int heap_trace_period = 3;  // Dump heap trace every 3 cycles
    // Start heap trace
    esp_err_t heap_trace_result;
#endif
#if _DEVICE_ENABLE_STATUS_SYSINFO_MQTT
    int mqtt_cycle = 0;
    const int mqtt_period = 2;  // Dump system status every 2 cycles
    // Start heap trace
    esp_err_t mqtt_result;
#endif
#if _DEVICE_ENABLE_STATUS_MEMGUARD
    int consecutive_below_threshold_count = 0;
#endif
    while (1) {
        ESP_LOGI(STATUS_TAG, "=== System Status ===");
        cycle++;

#if _DEVICE_ENABLE_STATUS_SYSINFO_HEAP
        ESP_LOGI(STATUS_TAG, "--- Heap Information ---");
        // Print heap information
        heap_caps_print_heap_info(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        // Print free memory
        size_t free_heap = esp_get_free_heap_size();
        ESP_LOGI(STATUS_TAG, "Free heap: %u bytes", free_heap);

        // Print minimum free heap size (i.e., lowest value during program execution)
        size_t min_free_heap = esp_get_minimum_free_heap_size();
        ESP_LOGI(STATUS_TAG, "Minimum free heap size: %u bytes", min_free_heap);
#endif

#if _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_CHECK
        ESP_LOGI(STATUS_TAG, "--- Checking heap integrity ---");
        // Check heap integrity
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(STATUS_TAG, "Heap corruption detected!");
        } else {
            ESP_LOGI(STATUS_TAG, "No heap corruption detected");
        }
#endif

        /* DISABLED -- sometimes causes crash 
        // Dump heap trace after some time (e.g., every 3 heap monitor cycles)
        if (cycle > period && heap_trace_stop() == ESP_OK) {
            ESP_LOGI(TAG, "Heap trace stopped. Dumping results...");
            heap_trace_dump();  // Dump the trace logs to check for leaks
            // Restart heap trace after dump
            heap_trace_start(HEAP_TRACE_LEAKS);  
        }
         */
#if _DEVICE_ENABLE_STATUS_SYSINFO_MQTT
        if (mqtt_cycle > mqtt_period) {
            // Post system status. Function mqtt_publish_system_info() will check if MQTT is enabled and connected
            ESP_LOGI(STATUS_TAG, "--- Publishing system status to MQTT ---");
            sensor_status_t status;
            if (sensor_status_init(&status) == ESP_OK) {
                if (mqtt_publish_system_info(&status) != ESP_OK) {
                    ESP_LOGE(STATUS_TAG, "Failed to publish system status to MQTT");
                } else {
                    ESP_LOGI(STATUS_TAG, "System status published to MQTT");
                }
            } else {
                ESP_LOGE(STATUS_TAG, "Failed to initialize device status");
            }
            mqtt_cycle = 0;
        }
#endif

#if _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_TRACE
        // Dump heap trace after some time (e.g., every 3 heap monitor cycles)
        if (heap_trace_cycle > heap_trace_period) {
            heap_trace_result = heap_trace_stop();
            if (heap_trace_result != ESP_OK) {
                ESP_LOGE(STATUS_TAG, "Failed to stop heap trace: %s", esp_err_to_name(heap_trace_result));
            } else {
                ESP_LOGI(STATUS_TAG, "Heap trace stopped successfully");
                ESP_LOGI(STATUS_TAG, "Dumping results...");
                heap_trace_dump();  // Dump the trace logs to check for leaks
                // Restart heap trace after dump
                heap_trace_result = heap_trace_start(HEAP_TRACE_LEAKS);  
                if (heap_trace_result != ESP_OK) {
                    ESP_LOGE(STATUS_TAG, "Failed to restart heap trace: %s", esp_err_to_name(heap_trace_result));
                } else {
                    ESP_LOGI(STATUS_TAG, "Heap trace restarted for leak detection");
                }
                heap_trace_cycle = 0;
            }
        }
#endif

#if _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_TRACE
        heap_trace_cycle++;
#endif
#if _DEVICE_ENABLE_STATUS_SYSINFO_MQTT
        mqtt_cycle++;
#endif
        // Delay for a period (e.g., 10 seconds)
        vTaskDelay(pdMS_TO_TICKS(HEAP_DUMP_INTERVAL_MS));
    }
}

void status_init() {
#if _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_TRACE    
    // Initialize heap tracing with a standalone buffer
    esp_err_t err = heap_trace_init_standalone(trace_buffer, NUM_RECORDS);
    if (err != ESP_OK) {
        ESP_LOGE(STATUS_TAG, "Heap trace initialization failed: %s", esp_err_to_name(err));
        return;
    } else {
        ESP_LOGI(STATUS_TAG, "Heap trace enable and initialized successfully");
    }

    // Start heap trace with leak detection enabled
    heap_trace_start(HEAP_TRACE_LEAKS);
    ESP_LOGI(STATUS_TAG, "Heap trace started for leak detection");
#endif

    // Start monitoring task
    ESP_LOGI(STATUS_TAG, "Starting system status task");
    xTaskCreate(status_task, "status_task", 4096, NULL, 1, NULL); // 4096 bytes of stack space
}

/**
 * @brief: Initialize sensor device status
 */
esp_err_t sensor_status_init(sensor_status_t *status_data) {

    status_data->free_heap = esp_get_free_heap_size();
    status_data->min_free_heap = esp_get_minimum_free_heap_size();
    status_data->time_since_boot = esp_timer_get_time();
#if _DEVICE_ENABLE_STATUS_MEMGUARD
    uint32_t memguard_threshold = S_DEFAULT_STATUS_MEMGUARD_THRESHOLD;
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_THRESHOLD, &memguard_threshold));
    status_data->memguard_threshold = (size_t)memguard_threshold;
    uint16_t memguard_mode = S_DEFAULT_STATUS_MEMGUARD_MODE;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_STATUS_MEMGUARD_MODE, &memguard_mode));
    status_data->memguard_mode = memguard_mode;
#endif

    ESP_LOGD(STATUS_TAG, "Device status initialized: Free heap (%u bytes), Min free heap (%u bytes), Time since boot (%llu microseconds)", 
             status_data->free_heap, status_data->min_free_heap, (unsigned long long)status_data->time_since_boot);

    return ESP_OK;
}