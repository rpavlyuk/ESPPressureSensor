#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>
#include "cJSON.h"
#include "esp_system.h"
#include "common.h"

#define HEAP_DUMP_INTERVAL_MS 10000  // 10 seconds
#define NUM_RECORDS 100  // Number of allocations to trace
#define BACKTRACE_DEPTH 6  // Number of stack frames to capture in backtrace

#define MEMGUARD_BOOT_PROTECTION_TIME_MINUTES   3  // Minimum uptime in minutes before allowing reboot
#define MEMGUARD_CONSECUTIVE_THRESHOLD_COUNT    3  // Number of consecutive checks below threshold before action
#define MEMGUARD_REBOOT_FUNCTION esp_restart  // Function to call for rebooting. Options: system_reboot, esp_restart, abort or custom

static const char *STATUS_TAG = "S HeapMonitor";

/**
 * Sensor readings information
 */
typedef struct {
    size_t free_heap;
    size_t min_free_heap;
    int64_t time_since_boot;
    size_t memguard_threshold;
    uint16_t memguard_mode;
} sensor_status_t;

void status_task(void *pvParameters);

void status_init(void);

esp_err_t sensor_status_init(sensor_status_t *status_data);

#endif // STATUS_H
