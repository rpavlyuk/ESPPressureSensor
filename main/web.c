
#include <ctype.h>
#include "esp_spiffs.h"  // Include for SPIFFS
#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "esp_http_server.h"
#include "non_volatile_storage.h"

#include "common.h"
#include "settings.h"
#include "sensor.h"
#include "zigbee.h"
#include "web.h"
#include "status.h"
#include "hass.h"

void init_filesystem() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information");
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}


void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        httpd_uri_t config_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = config_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &config_uri);

        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t submit_uri = {
            .uri       = "/submit",
            .method    = HTTP_POST,
            .handler   = submit_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &submit_uri);

        // URI handler for reboot action
        httpd_uri_t reboot_uri = {
            .uri = "/reboot",
            .method = HTTP_POST,
            .handler = reboot_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &reboot_uri);

        // Register the Zigbee connect handler
        httpd_uri_t connect_zigbee_uri = {
            .uri       = "/connect-zigbee",
            .method    = HTTP_POST,
            .handler   = connect_zigbee_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &connect_zigbee_uri);

        // Register the status web service handler
        httpd_uri_t status_webserver_get_uri = {
            .uri       = "/status-data",
            .method    = HTTP_GET,
            .handler   = status_data_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_webserver_get_uri);      

    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}


static esp_err_t config_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing config web request");

    // empty message
    const char* message = "";

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
    FILE *f = fopen("/spiffs/config.html", "r");
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
    char *mqtt_server = NULL;
    char *mqtt_protocol = NULL;
    char *mqtt_user = NULL;
    char *mqtt_password = NULL;
    char *mqtt_prefix = NULL;
    char *ha_prefix = NULL;
    char *device_id = NULL;
    char *device_serial = NULL;

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

    // replace static fields
    assign_static_page_variables(html_output);


    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(device_id);
    free(device_serial);

    return ESP_OK;
}

static esp_err_t submit_post_handler(httpd_req_t *req) {
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
    FILE *f = fopen("/spiffs/config.html", "r");
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
    // We need to pre-allocate memory as we are loading those values from POST request
    char *mqtt_server = (char *)malloc(MQTT_SERVER_LENGTH);
    char *mqtt_protocol = (char *)malloc(MQTT_PROTOCOL_LENGTH);
    char *mqtt_user = (char *)malloc(MQTT_USER_LENGTH);
    char *mqtt_password = (char *)malloc(MQTT_PASSWORD_LENGTH);
    char *mqtt_prefix = (char *)malloc(MQTT_PREFIX_LENGTH);
    char *ha_prefix = (char *)malloc(HA_PREFIX_LENGTH);

    char mqtt_port_str[6], sensor_offset_str[10], sensor_linear_multiplier_str[10];
    char ha_upd_intervl_str[10];
    char sensor_samples_str[10];
    char sensor_smp_int_str[10];
    char sensor_deviate_str[10];
    char sensor_intervl_str[10];
    char mqtt_connect_str[10];

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

    // Decode potentially URL-encoded parameters
    url_decode(mqtt_server);
    url_decode(mqtt_protocol);
    url_decode(mqtt_user);
    url_decode(mqtt_password);
    url_decode(mqtt_prefix);
    url_decode(ha_prefix);

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

    /** Load and display settings */

    // Free pointers to previosly used strings
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);

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

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, &sensor_data.voltage_offset));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, &sensor_data.sensor_linear_multiplier));
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
    snprintf(mqtt_port_str, sizeof(mqtt_port_str), "%u", mqtt_port);
    snprintf(sensor_offset_str, sizeof(sensor_offset_str), "%.3f", sensor_data.voltage_offset);
    snprintf(sensor_linear_multiplier_str, sizeof(sensor_linear_multiplier_str), "%lu", sensor_data.sensor_linear_multiplier);
    snprintf(ha_upd_intervl_str, sizeof(ha_upd_intervl_str), "%li", (uint32_t) ha_upd_intervl);
    snprintf(sensor_samples_str, sizeof(sensor_samples_str), "%i", (uint16_t) sensor_samples);
    snprintf(sensor_smp_int_str, sizeof(sensor_smp_int_str), "%i", (uint16_t) sensor_smp_int);
    snprintf(sensor_deviate_str, sizeof(sensor_deviate_str), "%i", (uint16_t) sensor_deviate);
    snprintf(sensor_intervl_str, sizeof(sensor_intervl_str), "%i", (uint16_t) sensor_intervl);
    snprintf(mqtt_connect_str, sizeof(mqtt_connect_str), "%i", (uint16_t) mqtt_connect);

    // ESP_LOGI(TAG, "Current HTML output size: %i, MAX_TEMPLATE_SIZE: %i", sizeof(html_output), MAX_TEMPLATE_SIZE);

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
    replace_placeholder(html_output, "{VAL_MESSAGE}", success_message);
    replace_placeholder(html_output, "{VAL_HA_UPDATE_INTERVAL}", ha_upd_intervl_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_COUNT}", sensor_samples_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_INTERVAL}", sensor_smp_int_str);
    replace_placeholder(html_output, "{VAL_SENSOR_SAMPLING_MEDIAN_DEVIATION}", sensor_deviate_str);
    replace_placeholder(html_output, "{VAL_SENSOR_READ_INTERVAL}", sensor_intervl_str);
    replace_placeholder(html_output, "{VAL_MQTT_CONNECT}", mqtt_connect_str);

    // replace static fields
    assign_static_page_variables(html_output);

    // ESP_LOGI(TAG, "Final HTML output size: %i, MAX_TEMPLATE_SIZE: %i", sizeof(html_output), MAX_TEMPLATE_SIZE);

    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(device_id);
    free(device_serial);

    return ESP_OK;
}

// Helper function to fill in the static variables in the template
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
}

// Helper function to replace placeholders in the template
void replace_placeholder(char *html_output, const char *placeholder, const char *value) {
    char *pos;
    while ((pos = strstr(html_output, placeholder)) != NULL) {
        size_t len_before = pos - html_output;
        size_t len_placeholder = strlen(placeholder);
        size_t len_value = strlen(value);
        size_t len_after = strlen(pos + len_placeholder);

        // Shift the rest of the string to make space for the replacement value
        memmove(pos + len_value, pos + len_placeholder, len_after + 1);

        // Copy the replacement value into the position of the placeholder
        memcpy(pos, value, len_value);
    }
}

// Function to safely extract a single parameter value from the POST buffer
void extract_param_value(char *buf, const char *param_name, char *output, size_t output_size) {
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
    } else {
        output[0] = '\0';  // If not found, return an empty string
    }
}

// Helper function to convert a hexadecimal character to its decimal value
int hex2dec(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// URL decoding function
void url_decode(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        // Cast characters to unsigned char before using isxdigit
        if ((*src == '%') && isxdigit((unsigned char)*(src + 1)) && isxdigit((unsigned char)*(src + 2))) {
            // Convert the next two hexadecimal characters to the actual character
            *dst = (char)((hex2dec(*(src + 1)) << 4) | hex2dec(*(src + 2)));
            src += 3;  // Skip past the % and the two hex characters
        } else if (*src == '+') {
            *dst = ' ';  // Convert '+' to space
            src++;
        } else {
            *dst = *src;  // Copy character as-is
            src++;
        }
        dst++;
    }
    *dst = '\0';  // Null-terminate the decoded string
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

// Handle Zigbee connection request
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