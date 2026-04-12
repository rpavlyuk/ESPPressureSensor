/**
 * @file nvs_large.h
 * @brief Header file for handling large strings in NVS (Non-Volatile Storage) on ESP32.
 *
 * This component provides functions to write and read large strings to and from
 * the NVS by splitting them into manageable chunks. It includes metadata structures
 * and utility functions to handle chunking and reassembly of large strings.
 *
 * @note The maximum chunk size is defined to avoid hitting NVS limits.
 *
 * @author Roman Pavlyuk <roman@pavlyuk.consulting>
 */

#ifndef NVS_LARGE_H
#define NVS_LARGE_H

#include "esp_err.h"

typedef struct {
    int chunk_count;
    size_t content_len;
    char master_key[15];
    char chunk_meta_key[15];
} nvs_large_string_meta_t;

typedef struct {
    int index;
    char key[15];
    size_t size;
} nvs_large_string_chunk_meta_t;

#define MAX_CHUNK_SIZE      1900  // Maximum size of each chunk to avoid hitting NVS limits
#define NVS_L_TAG           "NVS_LARGE"


static esp_err_t generate_chunk_key(const char *base_key, size_t chunk_index, char *chunk_key, size_t chunk_key_size);

esp_err_t nvs_write_string_large(const char *namespace, const char *key, const char *value);
esp_err_t nvs_read_string_large(const char *namespace, const char *key, char **out_value);

char *substring(const char *src, size_t start, size_t length);
esp_err_t appendstring(char **total, const char *chunk);


#endif  // NVS_LARGE_H