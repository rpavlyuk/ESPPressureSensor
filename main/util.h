#ifndef UTIL_H
#define UTIL_H

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool attach_owned(cJSON *root, const char *key, cJSON *child);
char *print_and_delete(cJSON *root);

TickType_t nonzero_ticks(uint32_t ms);

int calculate_median(int* data, int size);

void wifi_provision_clear(void *buffer, size_t length);

bool is_minified_file(const char *file_name);
const char *content_type_from_ext(const char *path);
bool is_text_based_mimetype(const char *path);

#endif // UTIL_H