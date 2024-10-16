#ifndef _WEB_H
#define _WEB_H

#include "esp_http_server.h"

#define MAX_TEMPLATE_SIZE       16384
#define MAX_CA_CERT_SIZE        8192

/// @brief Initiate the SPIFFS
void init_filesystem();

void start_webserver(void);

static esp_err_t config_get_handler(httpd_req_t *req);
static esp_err_t submit_post_handler(httpd_req_t *req);
static esp_err_t reboot_handler(httpd_req_t *req);
static esp_err_t connect_zigbee_handler(httpd_req_t *req);
static esp_err_t status_data_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t ca_cert_post_handler(httpd_req_t *req);

void assign_static_page_variables(char *html_output);
void replace_placeholder(char *html_output, const char *placeholder, const char *value);
int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size);

int hex2dec(char c);
void url_decode(char *str);
void str_trunc_after(char *str, const char *lookup);

#endif