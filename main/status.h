#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>
#include "esp_system.h"
#include "common.h"

#define HEAP_DUMP_INTERVAL_MS 10000  // 10 seconds
#define NUM_RECORDS 100  // Number of allocations to trace
#define BACKTRACE_DEPTH 6  // Number of stack frames to capture in backtrace

static const char *STATUS_TAG = "S HeapMonitor";

/**
 * Sensor readings information
 */
typedef struct {
    size_t free_heap;
    size_t min_free_heap;
    int64_t time_since_boot;
} sensor_status_t;

void status_init(void);

esp_err_t sensor_status_init(sensor_status_t *status_data);

#endif // STATUS_H
