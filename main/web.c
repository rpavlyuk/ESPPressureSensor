
#include <ctype.h>
#include "esp_spiffs.h"  // Include for SPIFFS
#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "esp_http_server.h"
#include "non_volatile_storage.h"

#include "ca_cert_manager.h"

#include "version.h"
#include "common.h"
#include "settings.h"
#include "flags.h"
#include "sensor.h"
#include "zigbee.h"
#include "web.h"
#include "status.h"
#include "hass.h"
#include "mqtt.h"

static httpd_handle_t server = NULL;

/**
 * @brief: Run the HTTP server
 * 
 * This function creates an HTTP server and registers the necessary URI handlers.
 * It waits for the Wi-Fi to connect before starting the server.
 * 
 * @param[in] param: Task parameters (unused)
 * 
 * @return void No return value
 */
void run_http_server(void *param) {

    // wait for Wi-Fi to connect
    ESP_LOGI(TAG, "HTTPD Server: Waiting for Wi-Fi/network to become ready...");

    xEventGroupWaitBits(
        g_sys_events,            // event group handle
        BIT_WIFI_CONNECTED,      // bit(s) to wait for
        pdFALSE,                 // don’t clear the bit when unblocked
        pdTRUE,                  // wait until *all* bits are set (only one here)
        portMAX_DELAY            // wait forever
    );

    ESP_LOGI(TAG, "HTTPD Server: Wi-Fi/network is ready!");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if _DEVICE_ENABLE_WEB
    config.max_uri_handlers = 24;
#else
    config.max_uri_handlers = 16;
#endif
    config.stack_size = 16384;
    config.recv_wait_timeout = 20;
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started. Registering handlers...");
        esp_err_t err;
        int h_count = 0; // handler count
#if _DEVICE_ENABLE_WEB
        // Set WEB URI handlers
        httpd_uri_t config_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = config_get_handler,
            .user_ctx  = NULL
        };
        err = httpd_register_uri_handler(server, &config_uri);
        ESP_LOGI(TAG, "Register %s => %s", config_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        err = httpd_register_uri_handler(server, &status_uri);
        ESP_LOGI(TAG, "Register %s => %s", status_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t submit_uri = {
            .uri       = "/submit",
            .method    = HTTP_POST,
            .handler   = submit_config_handler,
            .user_ctx  = NULL
        };
        err = httpd_register_uri_handler(server, &submit_uri);
        ESP_LOGI(TAG, "Register %s => %s", submit_uri.uri, esp_err_to_name(err));
        h_count++;


        // URI handler for reboot action
        httpd_uri_t reboot_uri = {
            .uri = "/reboot",
            .method = HTTP_POST,
            .handler = reboot_handler,
            .user_ctx = NULL
        };
        err = httpd_register_uri_handler(server, &reboot_uri);
        ESP_LOGI(TAG, "Register %s => %s", reboot_uri.uri, esp_err_to_name(err));
        h_count++;

#if _DEVICE_ENABLE_ZIGBEE
        // Register the Zigbee connect handler
        httpd_uri_t connect_zigbee_uri = {
            .uri       = "/connect-zigbee",
            .method    = HTTP_POST,
            .handler   = connect_zigbee_handler,
            .user_ctx  = NULL
        };
        err = httpd_register_uri_handler(server, &connect_zigbee_uri);
        ESP_LOGI(TAG, "Register %s => %s", connect_zigbee_uri.uri, esp_err_to_name(err));
        h_count++;
#endif

        httpd_uri_t ca_cert_uri = {
            .uri       = "/ca-cert",
            .method    = HTTP_POST,
            .handler   = ca_cert_post_handler,
            .user_ctx  = NULL
        };

        // Register the handler
        err = httpd_register_uri_handler(server, &ca_cert_uri);  
        ESP_LOGI(TAG, "Register %s => %s", ca_cert_uri.uri, esp_err_to_name(err));
        h_count++;

        /** STATIC Handlers **/
        // Register the static file streaming handler
        httpd_uri_t static_uri = {
            .uri        = "/static/*",
            .method     = HTTP_GET,
            .handler    = static_stream_handler,
            .user_ctx   = NULL
        };

        err = httpd_register_uri_handler(server, &static_uri);
        ESP_LOGI(TAG, "Register %s => %s", static_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t ota_update_uri = {
            .uri      = "/ota-update",  // URL endpoint
            .method   = HTTP_POST,       // HTTP method
            .handler  = ota_post_handler, // Function to handle the request
            .user_ctx = NULL            // User context, if needed
        };

        // Register the OTA update URI handler
        err = httpd_register_uri_handler(server, &ota_update_uri);
        ESP_LOGI(TAG, "Register %s => %s", ota_update_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t reset_uri = {
            .uri      = "/reset",  // URL endpoint
            .method   = HTTP_POST,       // HTTP method
            .handler  = reset_post_handler, // Function to handle the request
            .user_ctx = NULL            // User context, if needed
        };

        // Register the reset URI handler
        err = httpd_register_uri_handler(server, &reset_uri);
        ESP_LOGI(TAG, "Register %s => %s", reset_uri.uri, esp_err_to_name(err));
        h_count++;
#endif

#if _DEVICE_ENABLE_HTTP_API
        // Register the status web service handler
        httpd_uri_t status_webserver_get_uri = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = status_data_handler,
            .user_ctx  = NULL
        };
        err = httpd_register_uri_handler(server, &status_webserver_get_uri);
        ESP_LOGI(TAG, "Register %s => %s", status_webserver_get_uri.uri, esp_err_to_name(err));
        h_count++;

                httpd_uri_t setting_update_uri = {
            .uri      = "/api/setting/update",  // URL endpoint
            .method   = HTTP_POST,       // HTTP method
            .handler  = set_setting_value_post_handler, // Function to handle the request
            .user_ctx = NULL            // User context, if needed
        };

        // Register the setting update URI handler
        err = httpd_register_uri_handler(server, &setting_update_uri);
        ESP_LOGI(TAG, "Register %s => %s", setting_update_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t setting_get_all_uri = {
            .uri      = "/api/setting/get/all",  // URL endpoint
            .method   = HTTP_GET,       // HTTP method
            .handler  = get_settings_all_handler, // Function to handle the request
            .user_ctx = NULL            // User context, if needed
        };

        // Register the setting get all URI handler
        err = httpd_register_uri_handler(server, &setting_get_all_uri);
        ESP_LOGI(TAG, "Register %s => %s", setting_get_all_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t setting_get_one_uri = {
            .uri      = "/api/setting/get",
            .method   = HTTP_GET,
            .handler  = get_setting_one_handler,
            .user_ctx = NULL
        };

        err = httpd_register_uri_handler(server, &setting_get_one_uri);
        ESP_LOGI(TAG, "Register %s => %s", setting_get_one_uri.uri, esp_err_to_name(err));
        h_count++;

        httpd_uri_t get_ca_cert_uri = {
            .uri      = "/api/cert/get",
            .method   = HTTP_GET,
            .handler  = get_ca_certificate_handler,
            .user_ctx = NULL
        };

        err = httpd_register_uri_handler(server, &get_ca_cert_uri);
        ESP_LOGI(TAG, "Register %s => %s", get_ca_cert_uri.uri, esp_err_to_name(err));
        h_count++;
#endif
        ESP_LOGI(TAG, "%d HTTP handlers registered. Server ready!", h_count);
    } else {
        ESP_LOGE(TAG, "Error starting HTTPD server!");
        return;
    }

    while(server) {
        vTaskDelay(5);
    }  
}

/**
 * @brief: Helper function to fill in the static variables in the template
 * 
 * This function replaces the placeholders in the HTML template with the actual values.
 * 
 * @param[in,out] html_output: The HTML template to modify
 * 
 * @return void No return value
 */
void assign_static_page_variables(char *html_output) {

    // replace size fields
    char f_len[10];
    snprintf(f_len, sizeof(f_len), "%i", MQTT_SERVER_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_SERVER}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_PROTOCOL_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_PROTOCOL}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_USER_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_USER}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_PASSWORD_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_PASSWORD}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_PREFIX_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_PREFIX}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", HA_PREFIX_LENGTH);
    replace_placeholder(html_output, "{LEN_HA_PREFIX}", f_len);
    
    snprintf(f_len, sizeof(f_len), "%li", (long int) HA_UPDATE_INTERVAL_MIN);
    replace_placeholder(html_output, "{MIN_HA_UPDATE_INTERVAL}", f_len);
    snprintf(f_len, sizeof(f_len), "%li", (long int) HA_UPDATE_INTERVAL_MAX);
    replace_placeholder(html_output, "{MAX_HA_UPDATE_INTERVAL}", f_len);
    snprintf(f_len, sizeof(f_len), "%.3f", SENSOR_OFFSET_MIN);
    replace_placeholder(html_output, "{MIN_SENSOR_OFFSET}", f_len);
    snprintf(f_len, sizeof(f_len), "%.3f", SENSOR_OFFSET_MAX);
    replace_placeholder(html_output, "{MAX_SENSOR_OFFSET}", f_len);

    snprintf(f_len, sizeof(f_len), "%li", (long int) SENSOR_LINEAR_MULTIPLIER_MIN);
    replace_placeholder(html_output, "{MIN_SENSOR_LINEAR_MULTIPLIER}", f_len);
    snprintf(f_len, sizeof(f_len), "%li", (long int) SENSOR_LINEAR_MULTIPLIER_MAX);
    replace_placeholder(html_output, "{MAX_SENSOR_LINEAR_MULTIPLIER}", f_len);


    snprintf(f_len, sizeof(f_len), "%i", SENSOR_SAMPLING_COUNT_MIN);
    replace_placeholder(html_output, "{MIN_SENSOR_SAMPLING_COUNT}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", SENSOR_SAMPLING_COUNT_MAX);
    replace_placeholder(html_output, "{MAX_SENSOR_SAMPLING_COUNT}", f_len);

    snprintf(f_len, sizeof(f_len), "%i", SENSOR_SAMPLING_INTERVAL_MIN);
    replace_placeholder(html_output, "{MIN_SENSOR_SAMPLING_INTERVAL}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", SENSOR_SAMPLING_INTERVAL_MAX);
    replace_placeholder(html_output, "{MAX_SENSOR_SAMPLING_INTERVAL}", f_len);

    snprintf(f_len, sizeof(f_len), "%i", SENSOR_SAMPLING_MEDIAN_DEVIATION_MIN);
    replace_placeholder(html_output, "{MIN_SENSOR_SAMPLING_MEDIAN_DEVIATION}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", SENSOR_SAMPLING_MEDIAN_DEVIATION_MAX);
    replace_placeholder(html_output, "{MAX_SENSOR_SAMPLING_MEDIAN_DEVIATION}", f_len);  

    snprintf(f_len, sizeof(f_len), "%i", SENSOR_READ_INTERVAL_MIN);
    replace_placeholder(html_output, "{MIN_SENSOR_READ_INTERVAL}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", SENSOR_READ_INTERVAL_MAX);
    replace_placeholder(html_output, "{MAX_SENSOR_READ_INTERVAL}", f_len);

    replace_placeholder(html_output, "{VAL_SW_VERSION}", DEVICE_SW_VERSION);
    replace_placeholder(html_output, "{VAL_SW_VERSION_NUM}", DEVICE_SW_VERSION_NUM);
    replace_placeholder(html_output, "{VAL_SW_BUILD_NUM}", DEVICE_SW_BUILD_NUM);

    snprintf(f_len, sizeof(f_len), "%i", CA_CERT_LENGTH);
    replace_placeholder(html_output, "{VAL_CA_CERT_LEN_MAX}", f_len);
}

/**
 * @brief: Helper function to replace placeholders in the template with actual values
 * 
 * This function replaces the placeholders in the HTML template with the actual values.
 * 
 * @param[in,out] html_output: The HTML template to modify
 * @param[in] placeholder: The placeholder to replace
 * @param[in] value: The value to insert
 * 
 * @return void No return value
 */ 
void replace_placeholder(char *html_output, const char *placeholder, const char *value) {
    char *pos;
    while ((pos = strstr(html_output, placeholder)) != NULL) {
        size_t len_placeholder = strlen(placeholder);
        size_t len_value = strlen(value);
        size_t len_after = strlen(pos + len_placeholder);

        // Shift the rest of the string to make space for the replacement value
        memmove(pos + len_value, pos + len_placeholder, len_after + 1);

        // Copy the replacement value into the position of the placeholder
        memcpy(pos, value, len_value);
    }
}

/**
 * @brief Replace placeholders in a bounded (sized) buffer.
 *
 * Safer alternative to replace_placeholder(): prevents buffer overflow and
 * avoids unbounded strlen() scans past the buffer capacity.
 *
 * Behavior:
 * - Modifies @p buf in place.
 * - Replaces ALL occurrences of @p placeholder with @p value.
 * - If expansion would exceed @p cap (including final '\0'), returns ESP_ERR_NO_MEM.
 *
 * @param[in,out] buf         Buffer containing a NUL-terminated string.
 * @param[in]     cap         Total buffer capacity in bytes (including space for '\0').
 * @param[in]     placeholder Placeholder substring to replace (must be non-empty).
 * @param[in]     value       Replacement string (NULL treated as "").
 *
 * @return
 *   ESP_OK on success
 *   ESP_ERR_INVALID_ARG on bad arguments or non-terminated input within cap
 *   ESP_ERR_NO_MEM if output would exceed cap
 */
esp_err_t replace_placeholder_sized(char *buf, size_t cap,
                                   const char *placeholder,
                                   const char *value) {
    if (!buf || cap == 0 || !placeholder) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!value) {
        value = "";
    }

    const size_t ph_len = strlen(placeholder);
    const size_t val_len = strlen(value);

    if (ph_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure buf is NUL-terminated within cap
    size_t cur_len = strnlen(buf, cap);
    if (cur_len >= cap) {
        return ESP_ERR_INVALID_ARG;
    }

    // Search from the beginning each time, but always bounded by cap via strnlen checks
    char *pos = buf;

    while (true) {
        // strstr() is fine as long as buf is terminated within cap (checked above)
        pos = strstr(pos, placeholder);
        if (!pos) {
            break;
        }

        // Re-evaluate current length (still bounded)
        cur_len = strnlen(buf, cap);
        if (cur_len >= cap) {
            return ESP_ERR_INVALID_ARG;
        }

        const size_t head_off = (size_t)(pos - buf);

        // Sanity: placeholder must fully reside within the current string
        if (head_off + ph_len > cur_len) {
            return ESP_ERR_INVALID_ARG;
        }

        // Tail length after the placeholder (bytes after it, not including '\0')
        const size_t tail_len = cur_len - (head_off + ph_len);

        // New length after replacement
        const size_t new_len = cur_len - ph_len + val_len;

        // Need space for '\0' too
        if (new_len >= cap) {
            return ESP_ERR_NO_MEM;
        }

        // Move tail (including '\0') to its new position
        memmove(buf + head_off + val_len,
                buf + head_off + ph_len,
                tail_len + 1);

        // Copy in replacement value
        if (val_len > 0) {
            memcpy(buf + head_off, value, val_len);
        }

        // Continue searching *after* the inserted value to avoid infinite loops
        pos = buf + head_off + val_len;
    }

    return ESP_OK;
}

/**
 * @brief Extracts the value of a specified parameter from a buffer (POST request).
 *
 * This function searches for a parameter name within a given buffer and extracts its corresponding value.
 * The extracted value is then stored in the provided output buffer.
 *
 * @param buf The buffer containing the parameters.
 * @param param_name The name of the parameter to search for.
 * @param output The buffer where the extracted parameter value will be stored.
 * @param output_size The size of the output buffer.
 * @return An integer providing the length of the extracted value, or 0 if the parameter is not found.
 */
int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size) {
    char *start = strstr(buf, param_name);
    if (start != NULL) {
        start += strlen(param_name);  // Move to the value part
        char *end = strchr(start, '&');  // Find the next '&'
        if (end == NULL) {
            end = start + strlen(start);  // If no '&' found, take till the end of the string
        }
        size_t len = end - start;
        if (len >= output_size) {
            len = output_size - 1;  // Ensure we don't overflow the output buffer
        }
        strncpy(output, start, len);
        output[len] = '\0';  // Null-terminate the result
        return len;  // Return the length of the extracted value
    } else {
        output[0] = '\0';  // If not found, return an empty string
        return 0;  // Return 0 length when not found
    }
}

/**
 * @brief Retrieves the value of a GET query parameter from an HTTP request.
 *
 * This function extracts the value of a specified query parameter from the URL of an HTTP request.
 * It allocates memory for the query string, retrieves it, and then searches for the specified parameter.
 * The extracted value is stored in the provided output buffer.
 *
 * @param req Pointer to the HTTP request structure.
 * @param name The name of the query parameter to retrieve.
 * @param out Buffer where the extracted parameter value will be stored.
 * @param out_sz Size of the output buffer.
 * @return ESP_OK if the parameter is found and extracted successfully,
 *         ESP_ERR_NOT_FOUND if the parameter is not found,
 *         ESP_ERR_INVALID_ARG if any argument is invalid,
 *         ESP_ERR_NO_MEM if memory allocation fails.
 */
static esp_err_t extract_param_value_from_get_query(httpd_req_t *req,
                                     const char *name,
                                     char *out,
                                     size_t out_sz)
{
    if (!req || !name || !out || out_sz == 0) return ESP_ERR_INVALID_ARG;

    out[0] = '\0';

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) return ESP_ERR_NOT_FOUND;

    char *q = malloc(qlen + 1);
    if (!q) return ESP_ERR_NO_MEM;

    esp_err_t err = httpd_req_get_url_query_str(req, q, qlen + 1);
    if (err != ESP_OK) {
        free(q);
        return err;
    }

    err = httpd_query_key_value(q, name, out, out_sz);
    free(q);

    return err; // ESP_OK or ESP_ERR_NOT_FOUND
}

// helper function that will find the last occurrence of the given substring (lookup) in the input string (str) and truncate the string at that point
void str_trunc_after(char *str, const char *lookup) {
    if (str == NULL || lookup == NULL) {
        return; // Return if either input is NULL
    }

    char *last_occurrence = NULL;
    char *current_position = str;

    // Find the last occurrence of the substring 'lookup' in 'str'
    while ((current_position = strstr(current_position, lookup)) != NULL) {
        last_occurrence = current_position;
        current_position += strlen(lookup); // Move past the current match
    }

    // If the 'lookup' substring was found, truncate the string after its last occurrence
    if (last_occurrence != NULL) {
        last_occurrence[strlen(lookup)] = '\0'; // Null-terminate after the last occurrence
    }
}

// Helper function to convert a hexadecimal character to its decimal value
int hex_to_dec(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// URL decoding function
void url_decode(char *src) {
    char *dst = src;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            // Convert hex to character
            *dst = (char)((hex_to_dec(src[1]) << 4) | hex_to_dec(src[2]));
            src += 2;
        } else if (*src == '+') {
            // Replace '+' with space
            *dst = ' ';
        } else {
            *dst = *src;
        }
        src++;
        dst++;
    }
    *dst = '\0'; // Null-terminate the decoded string
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
static const char *content_type_from_ext(const char *path) {
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
 * @brief Validates device identity from HTTP request query parameters.
 *
 * This function extracts the 'device_id' and 'device_serial' parameters from the HTTP request's query string
 * and compares them with the values stored in NVS (Non-Volatile Storage). If the values match, the function
 * returns ESP_OK; otherwise, it sends an appropriate HTTP error response and returns ESP_FAIL.
 *
 * @param req Pointer to the HTTP request structure.
 * @return ESP_OK if the device identity is valid, ESP_FAIL otherwise.
 */
static esp_err_t validate_device_identity_from_get_query(httpd_req_t *req)
{
    char device_id_in[DEVICE_ID_LENGTH + 1];
    char device_serial_in[DEVICE_SERIAL_LENGTH + 1];

    esp_err_t err;

    err = extract_param_value_from_get_query(req, "device_id", device_id_in, sizeof(device_id_in));
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device_id");
        return ESP_FAIL;
    }

    err = extract_param_value_from_get_query(req, "device_serial", device_serial_in, sizeof(device_serial_in));
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device_serial");
        return ESP_FAIL;
    }

    char *device_id_nvs = NULL;
    char *device_serial_nvs = NULL;

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id_nvs);
    if (err != ESP_OK || !device_id_nvs) {
        free(device_id_nvs);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_id from NVS");
        return ESP_FAIL;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial_nvs);
    if (err != ESP_OK || !device_serial_nvs) {
        free(device_id_nvs);
        free(device_serial_nvs);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_serial from NVS");
        return ESP_FAIL;
    }

    bool ok = (strcmp(device_id_in, device_id_nvs) == 0) &&
              (strcmp(device_serial_in, device_serial_nvs) == 0);

    free(device_id_nvs);
    free(device_serial_nvs);

    if (!ok) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Device ID or serial mismatch");
        return ESP_FAIL;
    }

    return ESP_OK;
}


/**
 * @brief Validates device identity from a cJSON object.
 *
 * This function extracts the 'device_id' and 'device_serial' fields from the provided cJSON object
 * and compares them with the values stored in NVS (Non-Volatile Storage). If the values match, the function
 * returns ESP_OK; otherwise, it returns ESP_FAIL.
 *
 * @param json Pointer to the cJSON object containing device identity fields.
 * @return ESP_OK if the device identity is valid, ESP_FAIL otherwise.
 */
static esp_err_t validate_device_identity_from_json(const cJSON *json) {
    if (!json) return ESP_FAIL;

    const cJSON *device_id_in = cJSON_GetObjectItemCaseSensitive(json, "device_id");
    const cJSON *device_serial_in = cJSON_GetObjectItemCaseSensitive(json, "device_serial");

    if (!cJSON_IsString(device_id_in) || (device_id_in->valuestring == NULL)) {
        return ESP_FAIL;
    }

    if (!cJSON_IsString(device_serial_in) || (device_serial_in->valuestring == NULL)) {
        return ESP_FAIL;
    }

    char *device_id_nvs = NULL;
    char *device_serial_nvs = NULL;

    esp_err_t err;

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id_nvs);
    if (err != ESP_OK || !device_id_nvs) {
        free(device_id_nvs);
        return ESP_FAIL;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial_nvs);
    if (err != ESP_OK || !device_serial_nvs) {
        free(device_id_nvs);
        free(device_serial_nvs);
        return ESP_FAIL;
    }

    bool ok = (strcmp(device_id_in->valuestring, device_id_nvs) == 0) &&
              (strcmp(device_serial_in->valuestring, device_serial_nvs) == 0);

    free(device_id_nvs);
    free(device_serial_nvs);

    if (!ok) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Converts a cJSON value to its string representation.
 *
 * This function takes a cJSON value and converts it to a string representation.
 * The resulting string is stored in the provided output buffer.
 *
 * @param v Pointer to the cJSON value to convert.
 * @param out Output buffer where the string representation will be stored.
 * @param out_sz Size of the output buffer.
 */
static void json_value_to_string(const cJSON *v, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    if (!v) {
        strlcpy(out, "<null>", out_sz);
        return;
    }

    if (cJSON_IsString(v)) {
        strlcpy(out, v->valuestring ? v->valuestring : "", out_sz);
    } else if (cJSON_IsNumber(v)) {
        // cJSON numbers are double internally; valueint is OK for ints
        // Use integer formatting if it looks integer-ish
        double d = v->valuedouble;
        if ((double)((int64_t)d) == d) {
            snprintf(out, out_sz, "%lld", (long long)((int64_t)d));
        } else {
            snprintf(out, out_sz, "%.6f", d);
        }
    } else if (cJSON_IsBool(v)) {
        strlcpy(out, cJSON_IsTrue(v) ? "true" : "false", out_sz);
    } else if (cJSON_IsNull(v)) {
        strlcpy(out, "null", out_sz);
    } else {
        // object/array/blob-ish: store compact JSON
        char *tmp = cJSON_PrintUnformatted((cJSON *)v);
        if (tmp) {
            strlcpy(out, tmp, out_sz);
            free(tmp);
        } else {
            strlcpy(out, "<unprintable>", out_sz);
        }
    }
}



/* WEB Handlers */
/**
* @brief HTTP GET handler for the configuration page.
*
* This function handles HTTP GET requests for the configuration page.
* It reads the HTML template from SPIFFS, replaces placeholders with actual values
* from NVS, and sends the resulting HTML content as the response.
*
* @param req Pointer to the HTTP request structure.
* @return ESP_OK on success, ESP_FAIL on failure.
*/
static esp_err_t config_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing config web request");
    ESP_LOGI(TAG, "Processing config web request");

    // empty message
    const char* message = "";

    // Prefer one big allocation to reduce fragmentation
    const size_t buf_size = MAX_LARGE_TEMPLATE_SIZE + 1;
    // const size_t total = buf_size * 2;
    const size_t total = buf_size;

    char *mem = (char *)malloc(total);

    if (!mem) {
        ESP_LOGE(TAG, "OOM: config handler needs %u bytes (free=%u, min_free=%u)",
                 (unsigned)total,
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "ESP device is out of free memory");
        return ESP_FAIL;
    }

    // Assign pointers within the allocated block
    char *html_output    = mem;

    // html_template[0] = '\0';
    html_output[0] = '\0';

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/config.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(mem);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_output
    size_t len = fread(html_output, 1, buf_size - 1, f);
    if (len == buf_size - 1 && !feof(f)) {
        ESP_LOGE(TAG, "config.html too large (>%u bytes)", (unsigned)(buf_size - 1));
        fclose(f);
        free(mem);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    fclose(f);
    html_output[len] = '\0'; // Null-terminate the string

    char *device_id = NULL;
    char *device_serial = NULL;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);

#if ENABLE_CONFIG_PLACEHOLDER_REPLACEMENT
    // Allocate memory for the strings you will retrieve from NVS
    char *mqtt_server = NULL;
    char *mqtt_protocol = NULL;
    char *mqtt_user = NULL;
    char *mqtt_password = NULL;
    char *mqtt_prefix = NULL;
    char *ha_prefix = NULL;

    uint16_t mqtt_connect;
    uint16_t mqtt_port;
    float sensor_offset;
    uint32_t sensor_linear_multiplier;

    uint32_t ha_upd_intervl;
    uint16_t sensor_samples;
    uint16_t sensor_smp_int;
    uint16_t sensor_deviate;

    uint16_t sensor_intervl;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, &sensor_offset));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, &sensor_linear_multiplier));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &sensor_samples));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &sensor_smp_int));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, &sensor_deviate));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &sensor_intervl));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connect));

    // Replace placeholders in the template with actual values
    char mqtt_port_str[6];
    char sensor_offset_str[10];
    char sensor_linear_multiplier_str[10];
    char ha_upd_intervl_str[10];
    char sensor_samples_str[10];
    char sensor_smp_int_str[10];
    char sensor_deviate_str[10];
    char sensor_intervl_str[10];
    char mqtt_connect_str[10];
    snprintf(mqtt_port_str, sizeof(mqtt_port_str), "%u", mqtt_port);
    snprintf(sensor_offset_str, sizeof(sensor_offset_str), "%.3f", sensor_offset);
    snprintf(sensor_linear_multiplier_str, sizeof(sensor_linear_multiplier_str), "%lu", sensor_linear_multiplier);
    snprintf(ha_upd_intervl_str, sizeof(ha_upd_intervl_str), "%li", (uint32_t) ha_upd_intervl);
    snprintf(sensor_samples_str, sizeof(sensor_samples_str), "%i", (uint16_t) sensor_samples);
    snprintf(sensor_smp_int_str, sizeof(sensor_smp_int_str), "%i", (uint16_t) sensor_smp_int);
    snprintf(sensor_deviate_str, sizeof(sensor_deviate_str), "%i", (uint16_t) sensor_deviate);
    snprintf(sensor_intervl_str, sizeof(sensor_intervl_str), "%i", (uint16_t) sensor_intervl);
    snprintf(mqtt_connect_str, sizeof(mqtt_connect_str), "%i", (uint16_t) mqtt_connect);

    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_MQTT_SERVER}", mqtt_server);
    replace_placeholder(html_output, "{VAL_MQTT_PORT}", mqtt_port_str);
    replace_placeholder(html_output, "{VAL_MQTT_PROTOCOL}", mqtt_protocol);
    replace_placeholder(html_output, "{VAL_MQTT_USER}", mqtt_user);
    replace_placeholder(html_output, "{VAL_MQTT_PASSWORD}", mqtt_password);
    replace_placeholder(html_output, "{VAL_MQTT_PREFIX}", mqtt_prefix);
    replace_placeholder(html_output, "{VAL_HA_PREFIX}", ha_prefix);
    replace_placeholder(html_output, "{VAL_SENSOR_OFFSET}", sensor_offset_str);
    replace_placeholder(html_output, "{VAL_SENSOR_LINEAR_MULTIPLIER}", sensor_linear_multiplier_str);
    replace_placeholder(html_output, "{VAL_MESSAGE}", message);
    replace_placeholder(html_output, "{VAL_HA_UPDATE_INTERVAL}", ha_upd_intervl_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_COUNT}", sensor_samples_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_INTERVAL}", sensor_smp_int_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_MEDIAN_DEVIATION}", sensor_deviate_str);
    replace_placeholder(html_output, "{VAL_SENSOR_READ_INTERVAL}", sensor_intervl_str);
    replace_placeholder(html_output, "{VAL_MQTT_CONNECT}", mqtt_connect_str);
    replace_placeholder(html_output, "{VAL_CA_CERT}", ca_cert);
#endif
    // replace static fields
    assign_static_page_variables(html_output);


    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(mem);
#if ENABLE_CONFIG_PLACEHOLDER_REPLACEMENT
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
#endif
    free(device_id);
    free(device_serial);


    return ESP_OK;
}

/**
 * @brief: Handler for the POST request to submit the configuration form
 * 
 * This function handles the POST request to submit the configuration form.
 * It extracts the form data, saves the parameters to NVS, and sends a response back to the client.
 * 
 * @param[in] req: The HTTP request object
 * 
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t submit_config_handler(httpd_req_t *req) {
    // Extract form data
    char buf[1024];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // See what we got from client
    ESP_LOGI(TAG, "Received request: %s", buf);

    // empty message
    const char* success_message = "<div class=\"alert alert-primary alert-dismissible fade show\" role=\"alert\"> Parameters saved successfully. A device reboot might be required for the setting to come into effect.<button type=\"button\" class=\"btn-close\" data-bs-dismiss=\"alert\" aria-label=\"Close\"></button></div>";
    
    // Prefer one big allocation to reduce fragmentation
    const size_t buf_size = MAX_LARGE_TEMPLATE_SIZE + 1;
    const size_t total = buf_size;

    char *mem = (char *)malloc(total);

    if (!mem) {
        ESP_LOGE(TAG, "OOM: config handler needs %u bytes (free=%u, min_free=%u)",
                 (unsigned)total,
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "ESP device is out of free memory");
        return ESP_FAIL;
    }

    // Assign pointers within the allocated block
    char *html_output      = mem;

    // NULL-terminate the output buffer
    html_output[0] = '\0';

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/config.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(mem);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_output
    size_t len = fread(html_output, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    if (len == buf_size - 1 && !feof(f)) {
        ESP_LOGE(TAG, "config.html too large (>%u bytes)", (unsigned)(buf_size - 1));
        free(mem);
        mem = NULL;
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    html_output[len] = '\0'; // Null-terminate the string

    // Allocate memory for the strings you will retrieve from NVS
    // We need to pre-allocate memory as we are loading those values from POST request
    char *mqtt_server = (char *)malloc(MQTT_SERVER_LENGTH);
    char *mqtt_protocol = (char *)malloc(MQTT_PROTOCOL_LENGTH);
    char *mqtt_user = (char *)malloc(MQTT_USER_LENGTH);
    char *mqtt_password = (char *)malloc(MQTT_PASSWORD_LENGTH);
    char *mqtt_prefix = (char *)malloc(MQTT_PREFIX_LENGTH);
    char *ha_prefix = (char *)malloc(HA_PREFIX_LENGTH);
    char *ota_update_url = (char *)malloc(OTA_UPDATE_URL_LENGTH);
    char *net_log_host = (char *)malloc(NET_LOGGING_HOST_LENGTH);

    char mqtt_port_str[6], sensor_offset_str[10], sensor_linear_multiplier_str[10];
    char ha_upd_intervl_str[10];
    char sensor_samples_str[10];
    char sensor_smp_int_str[10];
    char sensor_deviate_str[10];
    char sensor_intervl_str[10];
    char mqtt_connect_str[10];
    char net_log_type_str[6];
    char net_log_port_str[6];
    char net_log_stdout_str[6];
    char ota_upd_rescfg_str[6];

    // Extract parameters from the buffer
    extract_param_value(buf, "mqtt_server=", mqtt_server, MQTT_SERVER_LENGTH);
    extract_param_value(buf, "mqtt_protocol=", mqtt_protocol, MQTT_PROTOCOL_LENGTH);
    extract_param_value(buf, "mqtt_user=", mqtt_user, MQTT_USER_LENGTH);
    extract_param_value(buf, "mqtt_password=", mqtt_password, MQTT_PASSWORD_LENGTH);
    extract_param_value(buf, "mqtt_prefix=", mqtt_prefix, MQTT_PREFIX_LENGTH);
    extract_param_value(buf, "ha_prefix=", ha_prefix, HA_PREFIX_LENGTH);
    extract_param_value(buf, "mqtt_port=", mqtt_port_str, sizeof(mqtt_port_str));
    extract_param_value(buf, "sensor_offset=", sensor_offset_str, sizeof(sensor_offset_str));
    extract_param_value(buf, "sensor_multipl=", sensor_linear_multiplier_str, sizeof(sensor_linear_multiplier_str));
    extract_param_value(buf, "ha_upd_intervl=", ha_upd_intervl_str, sizeof(ha_upd_intervl_str));
    extract_param_value(buf, "sensor_samples=", sensor_samples_str, sizeof(sensor_samples_str));
    extract_param_value(buf, "sensor_smp_int=", sensor_smp_int_str, sizeof(sensor_smp_int_str));
    extract_param_value(buf, "sensor_deviate=", sensor_deviate_str, sizeof(sensor_deviate_str));
    extract_param_value(buf, "sensor_intervl=", sensor_intervl_str, sizeof(sensor_intervl_str));
    extract_param_value(buf, "mqtt_connect=", mqtt_connect_str, sizeof(mqtt_connect_str));
    extract_param_value(buf, "ota_update_url=", ota_update_url, OTA_UPDATE_URL_LENGTH);
    extract_param_value(buf, "net_log_host=", net_log_host, NET_LOGGING_HOST_LENGTH);
    extract_param_value(buf, "net_log_type=", net_log_type_str, sizeof(net_log_type_str));
    extract_param_value(buf, "net_log_port=", net_log_port_str, sizeof(net_log_port_str));
    extract_param_value(buf, "net_log_stdout=", net_log_stdout_str, sizeof(net_log_stdout_str));
    extract_param_value(buf, "ota_upd_rescfg=", ota_upd_rescfg_str, sizeof(ota_upd_rescfg_str));

    // Convert mqtt_port and sensor_offset to their respective types
    uint16_t mqtt_port = (uint16_t)atoi(mqtt_port_str);  // Convert to uint16_t
    uint32_t sensor_linear_multiplier = (uint32_t)atoi(sensor_linear_multiplier_str);
    float sensor_offset = strtof(sensor_offset_str, NULL);  // Convert to float
    uint32_t ha_upd_intervl = (uint32_t)atoi(ha_upd_intervl_str);
    uint16_t sensor_samples = (uint16_t)atoi(sensor_samples_str);
    uint16_t sensor_smp_int = (uint16_t)atoi(sensor_smp_int_str);
    uint16_t sensor_deviate = (uint16_t)atoi(sensor_deviate_str);
    uint16_t sensor_intervl = (uint16_t)atoi(sensor_intervl_str);
    uint16_t mqtt_connect = (uint16_t)atoi(mqtt_connect_str);
    uint16_t net_log_port = (uint16_t)atoi(net_log_port_str);
    uint16_t net_log_stdout = (uint16_t)atoi(net_log_stdout_str);
    uint16_t net_log_type = (uint16_t)atoi(net_log_type_str);
    uint16_t ota_upd_rescfg = (uint16_t)atoi(ota_upd_rescfg_str);

    // Decode potentially URL-encoded parameters
    url_decode(mqtt_server);
    url_decode(mqtt_protocol);
    url_decode(mqtt_user);
    url_decode(mqtt_password);
    url_decode(mqtt_prefix);
    url_decode(ha_prefix);
    url_decode(ota_update_url);
    url_decode(net_log_host);

    // dump parameters for debugging pursposes
    ESP_LOGI(TAG, "Received setting parameters");
    ESP_LOGI(TAG, "mqtt_server: %s", mqtt_server);
    ESP_LOGI(TAG, "mqtt_protocol: %s", mqtt_protocol);
    ESP_LOGI(TAG, "mqtt_user: %s", mqtt_user);
    ESP_LOGI(TAG, "mqtt_password: %s", mqtt_password);
    ESP_LOGI(TAG, "mqtt_prefix: %s", mqtt_prefix);
    ESP_LOGI(TAG, "ha_prefix: %s", ha_prefix);
    ESP_LOGI(TAG, "mqtt_port: %i", mqtt_port);
    ESP_LOGI(TAG, "sensor_offset: %f", sensor_offset);
    ESP_LOGI(TAG, "sensor_linear_multiplier: %lu", sensor_linear_multiplier);
    ESP_LOGI(TAG, "ha_upd_intervl: %li", ha_upd_intervl);
    ESP_LOGI(TAG, "sensor_samples: %i", sensor_samples);
    ESP_LOGI(TAG, "sensor_smp_int: %i", sensor_smp_int);
    ESP_LOGI(TAG, "sensor_deviate: %i", sensor_deviate);
    ESP_LOGI(TAG, "sensor_intervl: %i", sensor_intervl);
    ESP_LOGI(TAG, "mqtt_connect: %i", mqtt_connect);
    ESP_LOGI(TAG, "net_log_type: %s", net_log_type_str);
    ESP_LOGI(TAG, "net_log_host: %s", net_log_host);
    ESP_LOGI(TAG, "net_log_port: %s", net_log_port_str);
    ESP_LOGI(TAG, "net_log_stdout: %s", net_log_stdout_str);
    ESP_LOGI(TAG, "ota_update_url: %s", ota_update_url);
    ESP_LOGI(TAG, "ota_upd_rescfg: %s", ota_upd_rescfg_str);

    // Save parsed values to NVS or apply them directly
    ESP_ERROR_CHECK(nvs_write_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, sensor_offset));
    ESP_ERROR_CHECK(nvs_write_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, sensor_linear_multiplier));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_SERVER, mqtt_server));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, mqtt_port));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, mqtt_protocol));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_USER, mqtt_user));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, mqtt_password));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, mqtt_prefix));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_HA_PREFIX, ha_prefix));
    ESP_ERROR_CHECK(nvs_write_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, sensor_samples));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, sensor_smp_int));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, sensor_deviate));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, sensor_intervl));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, mqtt_connect));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_NET_LOGGING_HOST, net_log_host));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_TYPE, net_log_type));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_PORT, net_log_port));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_KEEP_STDOUT, net_log_stdout));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, ota_update_url));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_OTA_UPDATE_RESET_CONFIG, ota_upd_rescfg));

  

    // Free pointers to previosly used strings
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(ota_update_url);
    free(net_log_host);

#if ENABLE_CONFIG_PLACEHOLDER_REPLACEMENT
    /** Load and display settings */
    // declaring NULL pointers for neccessary variables
    char *device_id = NULL;
    char *device_serial = NULL;

    // resetting the pointers
    mqtt_server = NULL;
    mqtt_protocol = NULL;
    mqtt_user = NULL;
    mqtt_password = NULL;
    mqtt_prefix = NULL;
    ha_prefix = NULL;
    ota_update_url = NULL;
    net_log_host = NULL;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, &sensor_data.voltage_offset));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, &sensor_data.sensor_linear_multiplier));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connect));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &sensor_samples));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &sensor_smp_int));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, &sensor_deviate));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &sensor_intervl));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_NET_LOGGING_HOST, &net_log_host));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_TYPE, &net_log_type));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_PORT, &net_log_port));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_NET_LOGGING_KEEP_STDOUT, &net_log_stdout));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_update_url));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_OTA_UPDATE_RESET_CONFIG, &ota_upd_rescfg));


    // Replace placeholders in the template with actual values
    snprintf(mqtt_port_str, sizeof(mqtt_port_str), "%u", mqtt_port);
    snprintf(sensor_offset_str, sizeof(sensor_offset_str), "%.3f", sensor_data.voltage_offset);
    snprintf(sensor_linear_multiplier_str, sizeof(sensor_linear_multiplier_str), "%lu", sensor_data.sensor_linear_multiplier);
    snprintf(ha_upd_intervl_str, sizeof(ha_upd_intervl_str), "%li", (uint32_t) ha_upd_intervl);
    snprintf(sensor_samples_str, sizeof(sensor_samples_str), "%i", (uint16_t) sensor_samples);
    snprintf(sensor_smp_int_str, sizeof(sensor_smp_int_str), "%i", (uint16_t) sensor_smp_int);
    snprintf(sensor_deviate_str, sizeof(sensor_deviate_str), "%i", (uint16_t) sensor_deviate);
    snprintf(sensor_intervl_str, sizeof(sensor_intervl_str), "%i", (uint16_t) sensor_intervl);
    snprintf(mqtt_connect_str, sizeof(mqtt_connect_str), "%i", (uint16_t) mqtt_connect);
    sprintf(net_log_port_str, sizeof(net_log_port_str), "%i", net_log_port);
    sprintf(net_log_type_str, sizeof(net_log_type_str), "%i", net_log_type);
    sprintf(net_log_stdout_str, sizeof(net_log_stdout_str), "%i", net_log_stdout);
    sprintf(ota_upd_rescfg_str, sizeof(ota_upd_rescfg_str), "%i", ota_upd_rescfg);

    // ESP_LOGI(TAG, "Current HTML output size: %i, MAX_TEMPLATE_SIZE: %i", sizeof(html_output), MAX_TEMPLATE_SIZE);

    replace_placeholder(html_output, "{VAL_MESSAGE}", success_message);
    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_MQTT_CONNECT}", mqtt_connect_str);
    replace_placeholder(html_output, "{VAL_MQTT_SERVER}", mqtt_server);
    replace_placeholder(html_output, "{VAL_MQTT_PORT}", mqtt_port_str);
    replace_placeholder(html_output, "{VAL_MQTT_PROTOCOL}", mqtt_protocol);
    replace_placeholder(html_output, "{VAL_MQTT_USER}", mqtt_user);
    replace_placeholder(html_output, "{VAL_MQTT_PASSWORD}", mqtt_password);
    replace_placeholder(html_output, "{VAL_MQTT_PREFIX}", mqtt_prefix);
    replace_placeholder(html_output, "{VAL_HA_PREFIX}", ha_prefix);
    replace_placeholder(html_output, "{VAL_SENSOR_OFFSET}", sensor_offset_str);
    replace_placeholder(html_output, "{VAL_SENSOR_LINEAR_MULTIPLIER}", sensor_linear_multiplier_str);
    replace_placeholder(html_output, "{VAL_HA_UPDATE_INTERVAL}", ha_upd_intervl_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_COUNT}", sensor_samples_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_INTERVAL}", sensor_smp_int_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_MEDIAN_DEVIATION}", sensor_deviate_str);
    replace_placeholder(html_output, "{VAL_SENSOR_READ_INTERVAL}", sensor_intervl_str);
    replace_placeholder(html_output, "{VAL_NET_LOGGING_HOST}", net_log_host);
    replace_placeholder(html_output, "{VAL_NET_LOGGING_TYPE}", net_log_type_str);
    replace_placeholder(html_output, "{VAL_NET_LOGGING_PORT}", net_log_port_str);
    replace_placeholder(html_output, "{VAL_NET_LOGGING_STDOUT}", net_log_stdout_str);
    replace_placeholder(html_output, "{VAL_OTA_UPDATE_URL}", ota_update_url);
    replace_placeholder(html_output, "{VAL_OTA_UPDATE_RESET_CONFIG}", ota_upd_rescfg_str);
#endif
    // replace static fields
    assign_static_page_variables(html_output);

    // ESP_LOGI(TAG, "Final HTML output size: %i, MAX_TEMPLATE_SIZE: %i", sizeof(html_output), MAX_TEMPLATE_SIZE);

    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));


#if ENABLE_CONFIG_PLACEHOLDER_REPLACEMENT
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(ota_update_url);
    free(net_log_host);

    free(device_id);
    free(device_serial);
#endif

    // Free dynamically allocated memory
    free(mem);
    return ESP_OK;
}

static esp_err_t reboot_handler(httpd_req_t *req) {
    ESP_LOGI("Reboot", "Rebooting the device...");

    // Send HTML response with a redirect after 30 seconds
    const char *reboot_html = "<html>"
                                "<head>"
                                    "<title>Rebooting...</title>"
                                    "<meta http-equiv=\"refresh\" content=\"30;url=/\" />"
                                    "<script>"
                                        "setTimeout(function() { window.location.href = '/'; }, 30000);"
                                    "</script>"
                                "</head>"
                                "<body>"
                                    "<h2>Device is rebooting...</h2>"
                                    "<p>Please wait, you will be redirected to the <a href=\"/\">home page</a> in 30 seconds.</p>"
                                "</body>"
                              "</html>";

    // Send the response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, reboot_html, HTTPD_RESP_USE_STRLEN);

    // Delay a bit to allow the response to be sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Reboot the device
    esp_restart();
    return ESP_OK;
}

#if _DEVICE_ENABLE_ZIGBEE
/**
 * @brief Handler for initiating Zigbee connection/pairing.
 * This handler responds to a Zigbee connection request by sending an HTML response that informs the user about the initiation of the MQTT connection/pairing process. It also includes a redirect to the home page after a specified time interval.
 * Note: The actual Zigbee initialization logic should be implemented in the zigbee_init_sensor() function, which is currently commented out. You can replace the placeholder with the actual implementation to establish the Zigbee connection.
 * 
 * @param req Pointer to the HTTP request structure.
 * @return ESP_OK if the response is sent successfully, otherwise an appropriate error code.
 * 
 */
static esp_err_t connect_zigbee_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Received Zigbee connection request");

    // Send HTML response with a redirect after 30 seconds
    const char *connect_mqtt_html = "<html>"
                                "<head>"
                                    "<title>Connecting MQTT...</title>"
                                    "<meta http-equiv=\"refresh\" content=\"30;url=/\" />"
                                    "<script>"
                                        "setTimeout(function() { window.location.href = '/'; }, 5000);"
                                    "</script>"
                                "</head>"
                                "<body>"
                                    "<h2>MQTT connection / pairing has been initiated.</h2>"
                                    "<p>Please wait, you will be redirected to the <a href=\"/\">home page</a> in 5 seconds.</p>"
                                "</body>"
                              "</html>";

    // Call Zigbee initialization function
    // esp_err_t err = zigbee_init_sensor();

    esp_err_t err = ESP_OK;

    if (err == ESP_OK) {
        httpd_resp_send(req, connect_mqtt_html, HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_sendstr(req, "Failed to initialize Zigbee.");
    }

    return ESP_OK;
}
#endif

/**
 * @brief: Status web-service
 */
static esp_err_t status_data_handler(httpd_req_t *req) {
    
    // Assuming you have a global or accessible structure containing sensor data
    sensor_data_t sensor_data = get_sensor_data();  // Example: Get sensor data
    sensor_status_t sensor_status;
    ESP_ERROR_CHECK(sensor_status_init(&sensor_status));
    
    // Convert cJSON object to a string
    const char *json_response = serialize_all_device_data(&sensor_status, &sensor_data);

    
    // Set the content type to JSON and send the response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));

    // Free allocated memory
    free((void *)json_response);

    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing status web request");

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);

    if (html_template == NULL || html_output == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/status.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(html_template);
        free(html_output);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);

    // Allocate memory for the strings you will retrieve from NVS
    char *device_id = NULL;
    char *device_serial = NULL;
    uint16_t sensor_intervl;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &sensor_intervl));

    char sensor_intervl_str[10];
    snprintf(sensor_intervl_str, sizeof(sensor_intervl_str), "%i", (uint16_t) sensor_intervl);

    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_SENSOR_READ_INTERVAL}", sensor_intervl_str);

    // replace static fields
    assign_static_page_variables(html_output);


    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(device_id);
    free(device_serial);

    return ESP_OK;
}

/**
 * @brief: Handler for the POST request to update the relay state
 * 
 * This function handles the POST request to update the relay state.
 * It extracts the relay ID and state from the request, updates the relay state, and sends a response back to the client.
 * 
 * @param[in] req: The HTTP request object
 * 
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t ca_cert_post_handler(httpd_req_t *req) {

    ESP_LOGI(TAG, "Processing certificate saving web request");

    // Buffer to hold the received certificate
    char buf[1024];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int total_len = req->content_len;
    ESP_LOGI(TAG, "Total POST content length: %d", total_len);
    if (total_len == 0) {
        ESP_LOGE(TAG, "POST content length is 0. Cannot proceed.");
        return ESP_FAIL;
    }
    int received = 0;

    // Prefer one big allocation to reduce fragmentation
    const size_t buf_size = MAX_SMALL_TEMPLATE_SIZE;
    const size_t total = buf_size * 2;

    char *mem = (char *)malloc(total);

    if (!mem) {
        ESP_LOGE(TAG, "OOM: config handler needs %u bytes (free=%u, min_free=%u)",
                 (unsigned)total,
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "ESP device is out of free memory");
        return ESP_FAIL;
    }

    // Split memory into 2 regions
    char *html_template    = mem;
    char *html_output      = mem + buf_size;

    html_template[0] = '\0';
    html_output[0] = '\0';

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/ca-cert-saving.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(mem);
        mem = NULL;
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_SMALL_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);


    // Allocate memory for the certificate outside the stack
    char *content = (char *)malloc(total_len + 1);
    if (content == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for CA certificate");
        free(mem);
        mem = NULL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    // Fill content with zeros to avoid garbage
    memset(content, 0, total_len + 1);

    // Read the certificate data from the request in chunks
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf, MIN(total_len - received, sizeof(buf)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGE(TAG, "Timeout while receiving POST data");
            } else {
                ESP_LOGE(TAG, "Error while receiving POST data: error %s, code %d", esp_err_to_name(ret), ret);
            }
            free(content); // Free the allocated memory in case of failure
            free(mem);
            mem = NULL;
            content = NULL;
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        memcpy(content + received, buf, ret);
        received += ret;
    }

    ESP_LOGI(TAG, "POST Content:\n%s", content);

    // Extract certificate type from the request: mqtts or https
    char ca_type[MAX_CA_CERT_SIZE];
    int ca_type_length = extract_param_value(content, "cert_type=", ca_type, MAX_CA_CERT_SIZE);
    if (ca_type_length <= 0) {
        ESP_LOGE(TAG, "Failed to extract CA certificate type from the received data");
        free(content);
        free(mem);
        mem = NULL;
        content = NULL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to extract certificate type");
        return ESP_FAIL;
    }

    // determine certificate key based on the type
    const char *ca_key = (strcmp(ca_type, "mqtts") == 0) ? "ca_cert_mqtts=" : "ca_cert_https=";
    ESP_LOGI(TAG, "Will use key (%s) to extract the certificate according to its type (%s)", ca_key, ca_type);

    // extract ca_cert from the output
    char *ca_cert = (char *)malloc(MAX_CA_CERT_SIZE);
    // Extract the certificate
    int cert_length = extract_param_value(content, ca_key, ca_cert, MAX_CA_CERT_SIZE);
    if (cert_length <= 0) {
        ESP_LOGE(TAG, "Failed to extract CA certificate from the received data");
        free(content);
        free(ca_cert);
        free(mem);
        mem = NULL;
        content = NULL;
        ca_cert = NULL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to extract certificate");
        return ESP_FAIL;
    }

    // Decode the URL-encoded certificate
    url_decode(ca_cert);

    // Null-terminate the decoded certificate
    str_trunc_after(ca_cert, "-----END CERTIFICATE-----");

    // Save the certificate
    const char *ca_path = (strcmp(ca_type, "mqtts") == 0) ? CA_CERT_PATH_MQTTS : CA_CERT_PATH_HTTPS;
    ESP_LOGI(TAG, "Saving certificate to %s", ca_path);
    replace_placeholder(html_output, "{VAL_CA_PATH}", ca_path);

    esp_err_t err = save_ca_certificate(ca_cert, ca_path, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save CA certificate");
        free(content);
        free(ca_cert);
        free(mem);
        mem = NULL;
        content = NULL;
        ca_cert = NULL;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save certificate");
        return ESP_FAIL;
    }

    free(content);
    free(ca_cert); // Free the allocated memory after saving

    // Send a response indicating success
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_output);
    ESP_LOGI(TAG, "CA certificate saving request processed successfully");

    free(mem);
    mem = NULL;
    content = NULL;
    ca_cert = NULL;

    return ESP_OK;
}


/**
 * @brief Handler for /api/setting/update endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t set_setting_value_post_handler(httpd_req_t *req) {
    /**
     * Request JSON format:
     * {
            "device_id": "<DEVICE_ID>",
            "device_serial": "<DEVICE_SERIAL>"
            "data": {
                <"setting_key":   "setting_value",>
            },
            "action": <code> // 0 - no action, 1 - reboot if no errors, 2 - force reboot
        }
     * 
     * Request JSON example:
      {
            "device_id": "9C9E6E0D8C5C",
            "device_serial": "VU7303USWVEP6ENQ3POTTFHVV7JH97QX"
            data: {
                "ota_update_url":   "http://localhost:8080/ota/relayboard.bin",
                "ha_upd_intervl":   60000
            },
            "action": 1
        }
     */
    esp_err_t err;

    char content[MAX_JSON_BUFFER_SIZE];
    // Get the POST data
    int total_len = req->content_len;
    int received = 0;
    if (total_len >= sizeof(content)) {
        ESP_LOGE(TAG, "Content size overflowing the buffer!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    while (received < total_len) {
        int ret = httpd_req_recv(req, content + received, total_len - received);
        if (ret <= 0) {
            ESP_LOGE(TAG, "Unexpected error while reading from request: %i", ret);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        received += ret;
    }
    content[received] = '\0';

    // Log request content
    ESP_LOGI(TAG, "Received settings update request: %s", content);

    // Parse the incoming JSON data
    cJSON *json_request = cJSON_Parse(content);
    if (json_request == NULL) {
        ESP_LOGE(TAG, "Settings update: Failed to parse JSON request");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format");
        return ESP_FAIL;
    }

    // Validate of device_id and device_serial match the stored values
    // 1 - get device_id and device_serial from JSON
    cJSON *device_id_item = cJSON_GetObjectItem(json_request, "device_id");
    cJSON *device_serial_item = cJSON_GetObjectItem(json_request, "device_serial");
    if (device_id_item == NULL || !cJSON_IsString(device_id_item) ||
        device_serial_item == NULL || !cJSON_IsString(device_serial_item)) {
        ESP_LOGE(TAG, "Settings update: Missing or invalid 'device_id' or 'device_serial' in JSON request");
        cJSON_Delete(json_request);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'device_id' or 'device_serial'");
        return ESP_FAIL;
    }
    // 2 - read actual values from NVS
    char *device_id_nvs = NULL;
    char *device_serial_nvs = NULL;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id_nvs));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial_nvs));
    // 3 - compare
    if (strcmp(device_id_item->valuestring, device_id_nvs) != 0 ||
        strcmp(device_serial_item->valuestring, device_serial_nvs) != 0) {
        ESP_LOGE(TAG, "Settings update: Device ID or serial mismatch");
        free(device_id_nvs);
        free(device_serial_nvs);
        cJSON_Delete(json_request);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Device ID or serial mismatch");
        return ESP_FAIL;
    }
    free(device_id_nvs);
    free(device_serial_nvs);

    // Get the 'data' array from JSON
    cJSON *data = cJSON_GetObjectItem(json_request, "data");
    if (data == NULL) {
        ESP_LOGE(TAG, "Settings update: No 'data' object in JSON request");
        cJSON_Delete(json_request);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format: missing 'data' object");
        return ESP_FAIL;
    }

    // Iterate over each setting in the 'data' object
    cJSON *setting = NULL;
    int success_count = 0;
    int failure_count = 0;
    int total_count = 0;

    // Response root
    cJSON *resp_root = cJSON_CreateObject();
    cJSON *resp_status = cJSON_CreateObject();
    cJSON *resp_details = cJSON_CreateObject();

    cJSON_AddItemToObject(resp_root, "status", resp_status);
    cJSON_AddItemToObject(resp_root, "details", resp_details);

    cJSON_ArrayForEach(setting, data) {
        setting_update_msg_t update_msg = {0};

        const char *setting_key = setting->string;
        if (setting_key == NULL) {
            ESP_LOGW(TAG, "Settings update: Encountered setting with NULL key, skipping");
            continue;
        }

        // log the setting being processed
        ESP_LOGI(TAG, "Settings update: Processing setting '%s'", setting_key);

        total_count++;

        // Apply the setting
        esp_err_t err = apply_setting(setting_key, setting, &update_msg);

        // Prepare per-key details object
        cJSON *one = cJSON_CreateObject();

        // old_value
        if (update_msg.has_old) {
            cJSON_AddStringToObject(one, "old_value", update_msg.old_value_str);
        } else {
            cJSON_AddNullToObject(one, "old_value");
        }

        // new_value (stringified)
        char new_value_str[128];
        json_value_to_string(setting, new_value_str, sizeof(new_value_str));
        cJSON_AddStringToObject(one, "new_value", new_value_str);

        // status: 0 success, 1 failed
        int status = (err == ESP_OK) ? 0 : 1;
        cJSON_AddNumberToObject(one, "status", status);

        // error_msg: include only on failure (or always, your call)
        const char *msg = update_msg.msg[0] ? update_msg.msg : esp_err_to_name(err);
        cJSON_AddStringToObject(one, "error_msg", msg);
        if (err != ESP_OK) {
            failure_count++;           
        } else {
            success_count++;
        }

        // Attach this key’s object
        cJSON_AddItemToObject(resp_details, setting_key, one);
    }

    // Fill the status block
    cJSON_AddNumberToObject(resp_status, "success", success_count);
    cJSON_AddNumberToObject(resp_status, "failed",  failure_count);
    cJSON_AddNumberToObject(resp_status, "total",   total_count);

    // now, lets process the action if any
    bool reboot_required = false;
    cJSON *action_item = cJSON_GetObjectItem(json_request, "action");
    if (action_item != NULL && cJSON_IsNumber(action_item)) {
        int action_code = action_item->valueint;
        if (action_code == 2) {
            // force reboot
            reboot_required = true;
            // notify in the response
            ESP_LOGW(TAG, "Settings update: Reboot required due to action code 2 (force reboot even on errors)");
        } else if (action_code == 1) {
            // reboot if no errors
            if (failure_count == 0) {
                reboot_required = true;
                ESP_LOGI(TAG, "Settings update: Reboot required due to action code 1 (reboot if no errors)");
            } else {
                ESP_LOGW(TAG, "Settings update: Reboot requested but not possible due to action code 1 (errors detected)");
            }
        } else {
            ESP_LOGI(TAG, "Settings update: No reboot action requested (action code 0)");
        }
    }

    // Serialize and send
    char *resp_str = cJSON_PrintUnformatted(resp_root);
    if (!resp_str) {
        cJSON_Delete(resp_root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to build response JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    free(resp_str);
    cJSON_Delete(resp_root);
    cJSON_Delete(json_request);

    if (reboot_required)
    {
        ESP_LOGW(TAG, "Settings update: Rebooting device as per request...");
        system_reboot(); // calling safe reboot function
    }

    return ESP_OK;
    /**
      Response format:
        {
            "status": {
                "success": <number of successfully updated settings>,
                "failed": <number of failed settings updates>,
                "total": <total number of settings in the request>
            },
            "details": {
                    "<setting_key>": {
                        "old_value": "<OLD_VALUE>",
                        "new_value": "<NEW_VALUE>",
                        "status": 0 // 0 = success, 1 = failed
                        "error_msg": "<ERROR_MESSAGE_IF_ANY>"
                    }
            }
        }   
     */
}   

/**
 * @brief Handler for /api/setting/get/all endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t get_settings_all_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing get all settings web request");

    // 1) validate device identity via query args
    if (validate_device_identity_from_get_query(req) != ESP_OK) {
        // validate_device_identity_from_get_query already sent HTTP error response
        return ESP_FAIL;
    }

    // 2) build settings JSON
    setting_update_msg_t msg = {0};
    cJSON *root = get_all_settings_value_JSON(&msg);
    if (!root) {
        ESP_LOGE(TAG, "Failed to build settings JSON: %s (%s)",
                 msg.msg[0] ? msg.msg : "unknown",
                 esp_err_to_name(msg.err_code));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to build settings JSON");
        return ESP_FAIL;
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to serialize JSON");
        return ESP_FAIL;
    }

    // 3) send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    free(json_str);
    
    return ESP_OK;
}

/**
 * @brief Handler for /api/setting/get?key= endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t get_setting_one_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Processing get single setting web request");

    // 1) Validate device identity (device_id + device_serial in query string)
    if (validate_device_identity_from_get_query(req) != ESP_OK) {
        return ESP_FAIL; // helper already sent response
    }

    // 2) Extract 'key' parameter
    char key[64];
    esp_err_t err = extract_param_value_from_get_query(req, "key", key, sizeof(key));
    if (err != ESP_OK || key[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing key");
        return ESP_FAIL;
    }

    // Optional hardening: only allow safe characters in key
    // (prevents weird injection into logs / filenames if you ever use key elsewhere)
    for (const char *p = key; *p; p++) {
        const char c = *p;
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            (c == '_') || (c == '-');
        if (!ok) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid key format");
            return ESP_FAIL;
        }
    }

    // 3) Build JSON for that setting
    setting_update_msg_t msg = {0};
    cJSON *root = get_setting_value_JSON(key, &msg);
    if (!root) {
        // As you specified: if key not found -> NULL
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND,
                           msg.msg[0] ? msg.msg : "Setting not found");
        return ESP_FAIL;
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to serialize JSON");
        return ESP_FAIL;
    }

    // 4) Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    free(json_str);
    return ESP_OK;
}

/**
 * @brief HTTP handler to stream /static/<name> as /spiffs/static-<name> (SPIFFS has no dirs)
 * This handler streams static files from SPIFFS with on-the-fly placeholder replacement.
 *
 * Example:
 *   GET /static/script.js  ->  /spiffs/static-script.js
 *
 * Notes:
 * - Uses line-by-line streaming with placeholder replacement.
 * - For placeholders that might span lines, you'll need a real streaming placeholder engine.
 * - Adjust STREAM_READ_LINE_SZ and STREAM_LINE_BUF_SZ as needed.
 * 
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t static_stream_handler(httpd_req_t *req) {
    // Basic URI validation
    const char *uri = req->uri;                 // e.g. "/static/script.js"
    const char *prefix = "/static/";
    const size_t prefix_len = strlen(prefix);

    ESP_LOGI(TAG, "Static file request: %s", uri);

    if (strncmp(uri, prefix, prefix_len) != 0 || uri[prefix_len] == '\0') {
        ESP_LOGW(TAG, "Invalid static file request: %s", uri);
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Disallow parent traversal and weird paths
    // (we don't support subdirs anyway; map "a/b.js" -> "a-b.js")
    char mapped_name[128] = {0};
    {
        const char *name = uri + prefix_len;    // "script.js" or "dir/file.js"
        size_t j = 0;

        for (size_t i = 0; name[i] != '\0' && j < sizeof(mapped_name) - 1; i++) {
            char c = name[i];

            // Reject obvious traversal
            if (c == '.' && name[i + 1] == '.') {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid path");
                return ESP_OK;
            }

            // SPIFFS mapping: replace '/' with '-' (SPIFFS has no dirs)
            if (c == '/') c = '-';

            // Keep it conservative: allow alnum and a few safe symbols
            if (!(isalnum((unsigned char)c) || c=='-' || c=='_' || c=='.')) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid characters in path");
                return ESP_OK;
            }

            mapped_name[j++] = c;
        }

        mapped_name[j] = '\0';

        if (mapped_name[0] == '\0') {
            httpd_resp_send_404(req);
            return ESP_OK;
        }
    }

    // Build actual SPIFFS path: /spiffs/static-<mapped_name>
    char filepath[192];
    snprintf(filepath, sizeof(filepath), "%s%s", STATIC_PATH_PREFIX, mapped_name);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "File not found: %s (uri=%s)", filepath, uri);
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Set content type based on requested URI extension (not SPIFFS name)
    httpd_resp_set_type(req, content_type_from_ext(uri));
#if ENABLE_STATIC_NOCACHE_HEADER
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
#else
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600"); // optional
#endif

    // Buffers
    char *read_line = (char *)malloc(STREAM_READ_LINE_SZ);
    char *work_line = (char *)malloc(STREAM_LINE_BUF_SZ);
    if (!read_line || !work_line) {
        fclose(f);
        free(read_line);
        free(work_line);
        httpd_resp_send_500(req);
        return ESP_OK;
    }

#if ENABLE_PLACEHOLDER_REPLACEMENT
    // NOTE: If you want these per-request, extract them from query params.
    // For now, keep placeholders consistent with the rest of your templating model.
    char *device_id = NULL;
    char *device_serial = NULL;

    esp_err_t err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
    if (err != ESP_OK || !device_id) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_id from NVS");
        return ESP_FAIL;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial);
    if (err != ESP_OK || !device_serial) {
        free(device_id);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_serial from NVS");
        return ESP_FAIL;
    }
 #endif

    while (fgets(read_line, STREAM_READ_LINE_SZ, f) != NULL) {
        // If the line is longer than STREAM_READ_LINE_SZ-1, fgets returns a partial line
        // (no '\n' and not EOF). This approach cannot safely template such lines.
        size_t rl = strlen(read_line);
        if (rl == STREAM_READ_LINE_SZ - 1 && read_line[rl - 1] != '\n' && !feof(f)) {
            ESP_LOGE(TAG,
                     "Line too long in %s; increase STREAM_READ_LINE_SZ/STREAM_LINE_BUF_SZ "
                     "or avoid templating large/minified assets",
                     filepath);
            fclose(f);
            free(read_line);
            free(work_line);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "template line too long");
            return ESP_OK;
        }

        // Copy into a larger working buffer to give replacements room to expand
        strlcpy(work_line, read_line, STREAM_LINE_BUF_SZ);

#if ENABLE_PLACEHOLDER_REPLACEMENT
        // Replace placeholders safely (bounded)
        esp_err_t r;

        r = replace_placeholder_sized(work_line, STREAM_LINE_BUF_SZ,
                                      "{VAL_DEVICE_ID}", device_id);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "Template expansion overflow in %s (device_id): %s",
                     filepath, esp_err_to_name(r));
            fclose(f);
            free(read_line);
            free(work_line);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "template expansion overflow");
            return ESP_OK;
        }

        r = replace_placeholder_sized(work_line, STREAM_LINE_BUF_SZ,
                                      "{VAL_DEVICE_SERIAL}", device_serial);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "Template expansion overflow in %s (device_serial): %s",
                     filepath, esp_err_to_name(r));
            fclose(f);
            free(read_line);
            free(work_line);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "template expansion overflow");
            return ESP_OK;
        }
#endif
        // Stream it out
        esp_err_t err = httpd_resp_send_chunk(req, work_line, HTTPD_RESP_USE_STRLEN);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "send_chunk failed (err=0x%x)", (unsigned)err);
            break;
        }

        // Optional: yield a tick if you ever template larger pages to avoid WDT starvation
        // vTaskDelay(1);
    }

    fclose(f);
    free(read_line);
    free(work_line);

#if ENABLE_PLACEHOLDER_REPLACEMENT
    free(device_id);
    free(device_serial);
#endif

    // End chunked response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Handler for /api/cert/get endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t get_ca_certificate_handler(httpd_req_t *req) {

    // Prepare JSON response root early (so every path returns consistent JSON)
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        // If even cJSON can't allocate, fall back to plain text 500
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OOM: failed to allocate JSON object");
        return ESP_FAIL;
    }

    // Defaults
    cJSON_AddStringToObject(root, "cert", "");
    cJSON_AddStringToObject(root, "type", "");
    cJSON_AddNumberToObject(root, "size", 0);
    cJSON_AddNumberToObject(root, "status", 0);
    cJSON_AddStringToObject(root, "msg", "");

    esp_err_t err = ESP_OK;

    // 1) Validate device identity (expects device_id & device_serial in query)
    err = validate_device_identity_from_get_query(req);
    if (err != ESP_OK) {
        cJSON_ReplaceItemInObject(root, "status", cJSON_CreateNumber((double)err));
        cJSON_ReplaceItemInObject(root, "msg", cJSON_CreateString("Device identity validation failed"));

        char *out = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!out) {
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "OOM: failed to serialize JSON");
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
        free(out);
        return ESP_OK;
    }

    // 2) Extract ca_type
    char ca_type[8] = {0}; // "https" or "mqtts"
    err = extract_param_value_from_get_query(req, "ca_type", ca_type, sizeof(ca_type));
    if (err != ESP_OK || ca_type[0] == '\0') {
        cJSON_ReplaceItemInObject(root, "status", cJSON_CreateNumber((double)err));
        cJSON_ReplaceItemInObject(root, "msg", cJSON_CreateString("missing or invalid ca_type"));

        char *out = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!out) {
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "OOM: failed to serialize JSON");
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
        free(out);
        return ESP_OK;
    }

    // 3) Map ca_type -> cert path
    const char *cert_type = NULL;
    const char *cert_path = NULL;

    if (strcmp(ca_type, "https") == 0) {
        cert_type = "https";
        cert_path = CA_CERT_PATH_HTTPS;
    } else if (strcmp(ca_type, "mqtts") == 0) {
        cert_type = "mqtts";
        cert_path = CA_CERT_PATH_MQTTS;
    } else {
        cJSON_ReplaceItemInObject(root, "status", cJSON_CreateNumber(3));
        cJSON_ReplaceItemInObject(root, "msg", cJSON_CreateString("ca_type must be 'https' or 'mqtts'"));

        char *out = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!out) {
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "OOM: failed to serialize JSON");
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
        free(out);
        return ESP_OK;
    }

    // 4) Load certificate (allocated by load_ca_certificate; must free)
    char *cert = NULL;
    err = load_ca_certificate(&cert, cert_path);
    if (err != ESP_OK || cert == NULL) {
        ESP_LOGW(TAG, "Failed to load CA cert type=%s (err=0x%x)", cert_type, (unsigned)err);

        cJSON_ReplaceItemInObject(root, "type", cJSON_CreateString(cert_type));
        cJSON_ReplaceItemInObject(root, "status", cJSON_CreateNumber(4));
        cJSON_ReplaceItemInObject(root, "msg", cJSON_CreateString("failed to load certificate"));

        char *out = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!out) {
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "OOM: failed to serialize JSON");
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
        free(out);

        if (cert) {
            free(cert);
        }
        return ESP_OK;
    }

    // 5) Build success JSON
    const size_t cert_len = strlen(cert);

    cJSON_ReplaceItemInObject(root, "type", cJSON_CreateString(cert_type));
    cJSON_ReplaceItemInObject(root, "size", cJSON_CreateNumber((double)cert_len));
    cJSON_ReplaceItemInObject(root, "cert", cJSON_CreateString(cert)); // cJSON will escape newlines properly

    // status=0 and msg="" already set

    // 6) Serialize and respond
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!out) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OOM: failed to serialize JSON");

        free(cert);
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);

    free(out);
    free(cert);

    return ESP_OK;
}

/**
 * @brief HTTP GET handler to trigger OTA update via web interface.
 *
 * This function is registered as an HTTP GET handler that triggers an OTA update
 * using the URL stored in NVS. The OTA update process is started, and if successful,
 * the device will send a response indicating the update has started and reboot afterward.
 *
 * @param req The HTTP request object.
 * @return ESP_OK on successful request handling, or an error code otherwise.
 */
static esp_err_t ota_post_handler(httpd_req_t *req) {
    char *ota_url = NULL;

    // Extract form data
    char buf[512];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int ret, remaining = req->content_len;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // dump received data for debugging
    ESP_LOGI(OTA_TAG, "Received OTA update request with data: %s", buf);

    char new_sw_version[64];
    int param_size = extract_param_value(buf, "new_sw_version=", new_sw_version, sizeof(new_sw_version));
    if (param_size <= 0) {
        // If the parameter is missing or empty, log a warning and use a default value
        strncpy(new_sw_version, "unknown", sizeof(new_sw_version));
        ESP_LOGW(OTA_TAG, "Failed to extract new_sw_version from request");
    } else {
        // clean the extracted version string (e.g., URL decode if needed)
        url_decode(new_sw_version);
    }
    
    ESP_LOGI(OTA_TAG, "Received OTA update request for new version: %s", new_sw_version);

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_SMALL_TEMPLATE_SIZE);

    if (html_template == NULL) {
        ESP_LOGE(OTA_TAG, "%s: Memory allocation failed", __func__);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/firmware-updating.html", "r");
    if (f == NULL) {
        ESP_LOGE(OTA_TAG, "%s: Failed to open file for reading", __func__);
        free(html_template);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string
    
    // Get the OTA URL from NVS
    if (nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_url) != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to read OTA URL from NVS");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(OTA_TAG, "Starting OTA via Web with URL: %s", ota_url);

    // replace values
    replace_placeholder(html_template, "{VAL_SW_FIRMWARE_URL}", ota_url);
    replace_placeholder(html_template, "{VAL_SW_VERSION_NEW}", new_sw_version);
        
    // replace static fields
    assign_static_page_variables(html_template);

    // Allocate memory for the task parameter
    ota_update_param_t *update_param = malloc(sizeof(ota_update_param_t));
    if (update_param == NULL) {
        ESP_LOGE(OTA_TAG, "%s: Memory allocation failed for OTA update parameters", __func__);
        free(html_template);
        free(ota_url);
        httpd_resp_send_500(req);  // Send error response in case of memory allocation failure
        return ESP_FAIL;
    }

    // Copy the OTA URL into the task parameter
    strlcpy(update_param->ota_url, ota_url, sizeof(update_param->ota_url));

    // Create the OTA update task, passing the OTA URL as the task parameter
    if (xTaskCreate(ota_update_task, "ota_update_task", 8192, update_param, 5, NULL) != pdPASS) {
        free(update_param);  // Free memory if task creation failed
        free(ota_url);
        free(html_template);
        httpd_resp_send_500(req);  // Send error response if task creation fails
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_template, strlen(html_template));


    // Free dynamically allocated memory if needed
    free(ota_url);
    free(html_template);
    return ESP_OK;
}

/**
 * @brief HTTP POST handler to reset the device to factory settings and/or reboot the device.
 *
 * This function is registered as an HTTP POST handler that resets the device to factory settings.
 * It reads the device ID and serial from NVS, validates them against the request, and performs the reset action.
 * The device will reboot after the reset action is completed.
 *
 * @param req The HTTP request object.
 * @return ESP_OK on successful request handling, or an error code otherwise.
 */
static esp_err_t reset_post_handler(httpd_req_t *req) {

    // Extract form data
    char buf[1024];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    char *device_id = NULL;
    char *device_serial = NULL;
    
    // Read stored device_id and device_serial
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    
    // Extract parameters from request
    char received_device_id[DEVICE_ID_LENGTH+1];
    char received_device_serial[DEVICE_SERIAL_LENGTH+1];
    char action_str[8];
    int action;

    // Get device_id and device_serial from request
    extract_param_value(buf, "device_id=", received_device_id, sizeof(received_device_id));
    extract_param_value(buf, "device_serial=", received_device_serial, sizeof(received_device_serial));
    
    // Retrieve action and convert to integer
    extract_param_value(buf, "action=", action_str, sizeof(action_str));
    action = (int)atoi(action_str);

    // Validate device_serial and device_id
    if (strcmp(device_id, received_device_id) != 0 || strcmp(device_serial, received_device_serial) != 0) {
        ESP_LOGE(TAG, "Device validation failed: mismatched device_id or device_serial.");
        ESP_ERROR_CHECK(httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Device validation failed"));
        return ESP_FAIL;
    }

    ret = ESP_FAIL;

    // Perform the appropriate reset action
    switch (action) {
        case 0:
            // Full factory reset
            ret = reset_factory_settings();
            break;
        case 1:
            // Reset device settings only (wifi settings preserved)
            ret = reset_device_settings();
            break;
        case 2:
            // Reset WiFi settings only (device settings preserved)
            //  NOTE: this will also disconnect from current WiFi network and the device will lose connectivity so the initizial network setup will be required again
            ret = reset_wifi_settings();
            break;
        case 9:
            // Just reboot the device, no other actions taken
            ret = system_reboot();
            break;
        default:
            ESP_LOGE(TAG, "Unknown action requested: %d", action);
            ESP_ERROR_CHECK(httpd_resp_send_404(req));
            return ESP_FAIL;
    }

    // Reboot the device if reset action was successful
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Action code %d completed successfully. Rebooting...", action);
        return reboot_handler(req);
    } else {
        ESP_LOGE(TAG, "Action code %d failed: %s", action, esp_err_to_name(ret));
        ESP_ERROR_CHECK(httpd_resp_send_500(req));
    }

    return ret;
}

/** HTTP Server control routines */

/**
 * @brief Stop the HTTP server
 * 
 * This function stops the HTTP server.
 * 
 * @param[in] server: The HTTP server handle
 * 
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t stop_http_server(httpd_handle_t server) {
    // Stop the HTTP server
    if (server != NULL) {
        ESP_ERROR_CHECK(httpd_stop(server));
        server = NULL;
    } else {
        ESP_LOGW(TAG, "NULL HTTP server handle");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/**
 * @brief Stop the HTTP server started in the "run_http_server" task
 * 
 * This function stops the HTTP server started in the "run_http_server" task.
 * 
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t http_stop(void) {
    return stop_http_server(server);
}
