#ifndef UTIL_H
#define UTIL_H

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool attach_owned(cJSON *root, const char *key, cJSON *child);
char *print_and_delete(cJSON *root);

TickType_t nonzero_ticks(uint32_t ms);

int calculate_median(int* data, int size);

#endif // UTIL_H