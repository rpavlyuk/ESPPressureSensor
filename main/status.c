#include "status.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"

static heap_trace_record_t trace_buffer[NUM_RECORDS];  // Buffer to store the trace records

void status_task(void *pvParameters) {
    int period = 3;
    int cycle = 0;
    while (1) {
        ESP_LOGI(STATUS_TAG, "=== System Status ===");
        cycle++;

        // Print heap information
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

        // Print free memory
        size_t free_heap = esp_get_free_heap_size();
        ESP_LOGI(STATUS_TAG, "Free heap: %u bytes", free_heap);

        // Print minimum free heap size (i.e., lowest value during program execution)
        size_t min_free_heap = esp_get_minimum_free_heap_size();
        ESP_LOGI(STATUS_TAG, "Minimum free heap size: %u bytes", min_free_heap);

        // check HEAP integrity
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(STATUS_TAG, "Heap corruption detected!");
        }
        else {
            ESP_LOGI(STATUS_TAG, "No heap corruption detected");
        }

        // Dump heap trace after some time (e.g., every 3 heap monitor cycles)
        if (cycle > period && heap_trace_stop() == ESP_OK) {
            ESP_LOGI(TAG, "Heap trace stopped. Dumping results...");
            heap_trace_dump();  // Dump the trace logs to check for leaks
            heap_trace_start(HEAP_TRACE_LEAKS);  // Restart heap trace after dump
        }


        // Delay for a period (e.g., 10 seconds)
        vTaskDelay(pdMS_TO_TICKS(HEAP_DUMP_INTERVAL_MS));
    }
}


void status_init() {
    // Initialize heap tracing with a standalone buffer
    esp_err_t err = heap_trace_init_standalone(trace_buffer, NUM_RECORDS);
    if (err != ESP_OK) {
        ESP_LOGE(STATUS_TAG, "Heap trace initialization failed: %s", esp_err_to_name(err));
        return;
    }

    // Start heap trace with leak detection enabled
    heap_trace_start(HEAP_TRACE_LEAKS);
    ESP_LOGI(STATUS_TAG, "Heap trace started for leak detection");

    // Start monitoring task
    ESP_LOGI(STATUS_TAG, "Starting system status task");
    xTaskCreate(status_task, "status_task", 4096, NULL, 1, NULL); // 4096 bytes of stack space
}
