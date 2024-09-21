#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>
#include "common.h"

#define HEAP_DUMP_INTERVAL_MS 10000  // 10 seconds
#define NUM_RECORDS 100  // Number of allocations to trace
#define BACKTRACE_DEPTH 6  // Number of stack frames to capture in backtrace

static const char *STATUS_TAG = "S HeapMonitor";

void status_init(void);

#endif // STATUS_H
