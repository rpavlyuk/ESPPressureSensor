/**
 * @file
 * @brief ESP-IDF component to store large strings in chunks in NVS.
 *
 * This component provides functionality to store and retrieve large strings as an extansion to the ESP32_NVS component.
 * by breaking them into smaller chunks and saving them in the Non-Volatile Storage (NVS).
 * This is useful for handling strings that exceed the size limit of a single NVS entry.
 *
 * Features:
 * - Split large strings into manageable chunks for storage.
 * - Retrieve and reassemble the chunks into the original string.
 * - Ensure data integrity and efficient storage management.
 *
 * Usage:
 * - Initialize the NVS storage.
 * - Use provided functions to store and retrieve large strings.
 *
 * Dependencies:
 * - ESP-IDF framework
 * - ESP32_NVS component (https://github.com/VPavlusha/ESP32_NVS)
 * 
 * @note The maximum chunk size is defined to avoid hitting NVS limits.
 * @author Roman Pavlyuk <roman@pavlyuk.consulting>
 *
 * Example:
 * @code
 * // Initialize NVS
 * nvs_init();
 * 
 * // Initialize the large string storage
 * char *large_string = "This is a large string to store in NVS."; // assuming this is a large string
 *
 * // Store a large string
 * nvs_write_string_large("my_namespace", "my_key", large_string);
 *
 * // Retrieve the large string
 * char *retrieved_string = NULL;
 * nvs_read_string_large("my_namespace", "my_key", &retrieved_string);
 * printf("Retrieved string: %s\n", retrieved_string);
 * @endcode
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include <nvs.h>
#include <nvs_flash.h>
#include "non_volatile_storage.h"
#include "nvs_large.h"


/**
 * @brief Generate a chunk key for a large string.
 * 
 * This function generates a chunk key for a large string by appending the chunk index to the base key.
 * 
 * @param[in] base_key Base key to use for chunk lookups.
 * @param[in] chunk_index Index of the chunk.
 * @param[out] chunk_key Buffer to store the generated chunk key.
 * @param[in] chunk_key_size Size of the chunk key buffer.
 * 
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
static esp_err_t generate_chunk_key(const char *base_key, size_t chunk_index, char *chunk_key, size_t chunk_key_size) {
    if (snprintf(chunk_key, chunk_key_size, "%s_%d", base_key, (int)chunk_index) < 0) {
        ESP_LOGE(NVS_L_TAG, "%s(): Failed to generate chunk key!", __func__);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Writes a large string to NVS in multiple chunks.
 * 
 * This function writes a large string to NVS by splitting it into multiple chunks.
 * The function calculates the number of chunks required and writes each chunk to NVS.
 * It also writes metadata to NVS to store the total number of chunks and the content length.
 * 
 * @param[in] namespace Namespace of the NVS key.
 * @param[in] key Base key to use for chunk lookups.
 * @param[in] value String value to write.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 * 
 */
esp_err_t nvs_write_string_large(const char *namespace, const char *key, const char *value) {

    if (namespace == NULL) {
        ESP_LOGE(NVS_L_TAG, "%s(): Failed to write string value: namespace is NULL!", __func__);
        return ESP_ERR_INVALID_ARG;
    }
    if (key == NULL) {
        ESP_LOGE(NVS_L_TAG, "%s(): Failed to write string value: key is NULL!", __func__);
        return ESP_ERR_INVALID_ARG;
    }
    if (value == NULL) {
        ESP_LOGE(NVS_L_TAG, "%s(): Failed to write string value: value is NULL!", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;

    nvs_large_string_meta_t str_meta = {
        .chunk_count = 0,
        .content_len = 0,
    };
    sprintf(str_meta.master_key, "%s", key);
    sprintf(str_meta.chunk_meta_key, "%s_M", key);

    size_t value_len = strlen(value);
    size_t chunk_count = (value_len + MAX_CHUNK_SIZE - 1) / MAX_CHUNK_SIZE;

    str_meta.chunk_count = chunk_count;
    str_meta.content_len = value_len;

    ESP_LOGD(NVS_L_TAG, "%s(): Content length: %li", __func__, (uint32_t)str_meta.content_len);
    ESP_LOGD(NVS_L_TAG, "%s(): Chunk count: %li", __func__, (uint32_t)str_meta.chunk_count);

    nvs_large_string_chunk_meta_t chunk_meta_array[chunk_count];

    for (size_t i = 0; i < chunk_count; i++) {

        nvs_large_string_chunk_meta_t chunk_meta = {
            .index = i,
            .size = 0,
        };

        char *chunk_value = substring(value, i * MAX_CHUNK_SIZE, MAX_CHUNK_SIZE);
        chunk_meta.size = strlen(chunk_value);

        generate_chunk_key(str_meta.master_key, i, chunk_meta.key, sizeof(chunk_meta.key));
        ESP_LOGI(NVS_L_TAG, "%s(): Iteration: %li, Chunk key: %s", __func__, (uint32_t)i, chunk_meta.key);
        
        esp_err_t err = nvs_write_string(namespace, chunk_meta.key, chunk_value);
        if (err != ESP_OK) {
            ESP_LOGE(NVS_L_TAG, "Failed to write chunk %s: %s", chunk_meta.key, esp_err_to_name(err));
            free(chunk_value);
            return err;
        } else {
            ESP_LOGD(NVS_L_TAG, "%s(): Wrote %li bytes to chunk %s", __func__, (uint32_t)chunk_meta.size, chunk_meta.key);
            ESP_LOGD(NVS_L_TAG, "%s(): Chunk written: %s", __func__, chunk_value);
        }
        free(chunk_value);

        // Write the chunk metadata
        chunk_meta_array[i] = chunk_meta;
    }

    // Write the chunk count as metadata
    err = nvs_write_blob(namespace, key, &str_meta, sizeof(nvs_large_string_meta_t));
    if (err != ESP_OK) {
        ESP_LOGE(NVS_L_TAG, "Failed to write large string metadata: %s", esp_err_to_name(err));
        return err;
    } else {
        ESP_LOGD(NVS_L_TAG, "%s(): Wrote string metadata: count (%i), size (%li), key (%s)", __func__, chunk_count, (uint32_t)value_len, key);
    }

    // Write the chunk metadata
    err = nvs_write_blob(namespace, str_meta.chunk_meta_key, chunk_meta_array, sizeof(nvs_large_string_chunk_meta_t) * chunk_count);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_L_TAG, "Failed to write large string chunk metadata: %s", esp_err_to_name(err));
        return err;
    } else {
        ESP_LOGD(NVS_L_TAG, "%s(): Wrote chunk metadata: count (%i), size (%li), key (%s)", __func__, chunk_count, (uint32_t)sizeof(nvs_large_string_chunk_meta_t) * chunk_count, str_meta.chunk_meta_key);
    }

    return err;
}

/**
 * @brief Reads a large string stored in NVS in multiple chunks and assembles it into a single string.
 *
 * This function reads a multi-part string from NVS, where the string is split across multiple keys.
 * It dynamically allocates and appends the chunks to form the complete string.
 *
 * @param[in] namespace Namespace of the NVS key.
 * @param[in] key Base key to use for chunk lookups.
 * @param[out] out_value Pointer to the buffer where the assembled string is stored. Must be pre-allocated or NULL.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t nvs_read_string_large(const char *namespace, const char *key, char **out_value) {
    if (!namespace || !key || !out_value) {
        ESP_LOGE(NVS_L_TAG, "Invalid arguments!");
        return ESP_ERR_INVALID_ARG;
    }

    *out_value = NULL;  // Initialize out_value

    // Read metadata
    nvs_large_string_meta_t str_meta;
    esp_err_t err = nvs_read_blob(namespace, key, &str_meta, sizeof(str_meta));
    if (err != ESP_OK) {
        ESP_LOGE(NVS_L_TAG, "Failed to read metadata: %s", esp_err_to_name(err));
        return err;
    }

    nvs_large_string_chunk_meta_t chunk_meta[str_meta.chunk_count];
    err = nvs_read_blob(namespace, str_meta.chunk_meta_key, chunk_meta, sizeof(chunk_meta));
    if (err != ESP_OK) {
        ESP_LOGE(NVS_L_TAG, "Failed to read chunk metadata: %s", esp_err_to_name(err));
        return err;
    }

    for (size_t i = 0; i < str_meta.chunk_count; i++) {
        char *chunk_value = NULL;
        err = nvs_read_string(namespace, chunk_meta[i].key, &chunk_value);
        if (err != ESP_OK) {
            ESP_LOGE(NVS_L_TAG, "Failed to read chunk %s: %s", chunk_meta[i].key, esp_err_to_name(err));
            free(*out_value);
            return err;
        }

        ESP_LOGD(NVS_L_TAG, "Read chunk %s: %s", chunk_meta[i].key, chunk_value);

        // Append chunk to the output
        err = appendstring(out_value, chunk_value);
        free(chunk_value);  // Free temp chunk value
        if (err != ESP_OK) {
            ESP_LOGE(NVS_L_TAG, "Failed to append chunk");
            free(*out_value);
            return err;
        }
    }

    ESP_LOGD(NVS_L_TAG, "Assembled string: %s", *out_value);
    return ESP_OK;
}


/**
 * Extracts a substring from a given string.
 * 
 * @param src The original string.
 * @param start The starting position in the source string (0-indexed).
 * @param length The maximum number of characters to extract.
 * @return A new null-terminated string containing the substring, or NULL if the allocation fails.
 */
char *substring(const char *src, size_t start, size_t length) {
    if (src == NULL || start >= strlen(src)) {
        return NULL;  // Invalid input or start is out of bounds.
    }

    // Calculate the actual length we can copy, considering the null terminator.
    size_t src_len = strlen(src);
    size_t actual_length = (start + length > src_len) ? (src_len - start) : length;

    // Allocate memory for the substring (+1 for null terminator).
    char *result = (char *)malloc(actual_length + 1);
    if (result == NULL) {
        return NULL;  // Memory allocation failed.
    }

    // Copy the substring and null-terminate it.
    strncpy(result, src + start, actual_length);
    result[actual_length] = '\0';

    return result;
}

/**
 * @brief Appends a chunk of string to the total string, dynamically reallocating memory.
 *
 * If `*total` is NULL, it initializes it as an empty string.
 * If `chunk` is NULL, it returns without modifying `*total`.
 *
 * @param[in,out] total Pointer to the total string. Memory is reallocated dynamically.
 * @param[in] chunk String to append to the total string.
 * @return
 *     - ESP_OK: Operation successful.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - ESP_ERR_INVALID_ARG: Invalid arguments.
 */
esp_err_t appendstring(char **destination, const char *source) {
    if (!destination || !source) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t dest_len = *destination ? strlen(*destination) : 0;
    size_t src_len = strlen(source);

    // Allocate memory for the combined string
    char *new_buffer = realloc(*destination, dest_len + src_len + 1);
    if (!new_buffer) {
        ESP_LOGE("APPEND", "Failed to reallocate memory");
        return ESP_ERR_NO_MEM;
    }

    *destination = new_buffer;  // Update destination with new buffer
    memcpy(new_buffer + dest_len, source, src_len + 1);  // Copy including null terminator

    return ESP_OK;
}

