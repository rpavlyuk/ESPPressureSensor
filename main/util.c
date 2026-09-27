/**
 * @file util.c
 * @brief Utility functions library
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "util.h"

/** JSON utility functions library for attaching and printing cJSON objects **/

/**
 * @brief Attaches a child cJSON object to a parent cJSON object with a specified key.
 * @param root The parent cJSON object.
 * @param key The key under which the child will be attached.
 * @param child The child cJSON object to attach.
 * @return true if the child was successfully attached, false otherwise.
 * 
 * @note The child object will be deleted if it cannot be attached to the parent.
 * @see cJSON_AddItemToObject
 */
bool attach_owned(cJSON *root, const char *key, cJSON *child)
{
    if (child == NULL) return false;
    if (!cJSON_AddItemToObject(root, key, child)) {
        cJSON_Delete(child);
        return false;
    }
    return true;
}

/**
 * @brief Prints a cJSON object to a string and deletes the cJSON object.
 * @param root The cJSON object to print and delete.
 * @return A string representation of the cJSON object, or NULL if the object is NULL.
 * 
 * @note The caller is responsible for freeing the returned string.
 * @see cJSON_Print
 */
char *print_and_delete(cJSON *root)
{
    if (root == NULL) return NULL;
    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    return text;
}


/** FreeRTOS utility helper functions **/
/**
 * @brief Converts milliseconds to FreeRTOS ticks, ensuring a non-zero value.
 * @param ms The time in milliseconds.
 * @return The equivalent time in FreeRTOS ticks, with a minimum of 1 tick.
 */
TickType_t nonzero_ticks(uint32_t ms) {
    TickType_t ticks = pdMS_TO_TICKS(ms);
    return ticks != 0 ? ticks : 1;
}

/** Mathematical utility functions **/

/**
 * @brief Calculates the median of an array of integers.
 * @param data The array of integers.
 * @param size The number of elements in the array.
 * @return The median value of the array, or 0 if the array is NULL or empty.
 */
int calculate_median(int* data, int size) {
    if (data == NULL || size <= 0) return 0;
    for (int i = 0; i < size - 1; ++i) {
        for (int j = i + 1; j < size; ++j) {
            if (data[i] > data[j]) {
                int tmp = data[i]; data[i] = data[j]; data[j] = tmp;
            }
        }
    }
    if (size % 2 != 0) return data[size / 2];
    return (int)(((int64_t)data[size / 2 - 1] + data[size / 2]) / 2);
}

/** WFI helper functions **/

/** 
 * @brief Clear temporary credential buffers before returning from the HTTP handler.
 * @param buffer The buffer to clear.
 * @param length The length of the buffer.
 */
void wifi_provision_clear(void *buffer, size_t length) {
    volatile unsigned char *p = buffer;
    while (length--) *p++ = 0;
}

/** File handling utility functions **/

/**
 * @brief Checks if a file is minified based on its name: returns true if it contains suffix ".min."
 * @param file_name The name of the file to check.
 * @return true if the file is minified, false otherwise.
 */
bool is_minified_file(const char *file_name) {
    if (file_name == NULL) return false;
    return strstr(file_name, ".min.") != NULL;
}

/**
 * @brief Determines the content type based on the file extension.
 *
 * This function takes a file path as input and returns the corresponding
 * MIME content type based on the file extension. If the extension is not
 * recognized, it defaults to "application/octet-stream".
 *
 * @param path The file path to analyze.
 * @return The corresponding content type as a string.
 */
const char *content_type_from_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    if (strcasecmp(dot, ".html") == 0) return "text/html";
    if (strcasecmp(dot, ".css")  == 0) return "text/css";
    if (strcasecmp(dot, ".js")   == 0) return "application/javascript";
    if (strcasecmp(dot, ".json") == 0) return "application/json";
    if (strcasecmp(dot, ".svg")  == 0) return "image/svg+xml";
    if (strcasecmp(dot, ".png")  == 0) return "image/png";
    if (strcasecmp(dot, ".jpg")  == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".ico")  == 0) return "image/x-icon";
    if (strcasecmp(dot, ".txt")  == 0) return "text/plain";

    return "application/octet-stream";
}

/**
 * @brief Determines if the MIME type of a file is text-based.
 *
 * This function checks the content type of the given file path and returns true
 * if it is a text-based MIME type (e.g., text/html, text/css, application/javascript, application/json).
 *
 * @param path The file path to analyze.
 * @return true if the MIME type is text-based, false otherwise.
 */
bool is_text_based_mimetype(const char *path) {
    const char *ctype = content_type_from_ext(path);
    return ctype && (strstr(ctype, "text/") == ctype || strcmp(ctype, "application/javascript") == 0 || strcmp(ctype, "application/json") == 0);
}
