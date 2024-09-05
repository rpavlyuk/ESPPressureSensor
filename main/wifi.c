#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"  // Use the appropriate scheme for provisioning
#include "esp_event.h"

#include "wifi.h"

char softap_ssid[32];       // Buffer for the generated SSID
char softap_password[64];   // Buffer for the generated password


void generate_softap_credentials() {
    uint8_t mac[6];  // Array to hold the MAC address
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Use esp_read_mac to get the MAC address

    // Extract the last 3 bytes (6 characters in hexadecimal) of the MAC address
    snprintf(softap_ssid, sizeof(softap_ssid), "PROV_AP_%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Ensure the password buffer is large enough to accommodate the SSID and "1234"
    snprintf(softap_password, sizeof(softap_password), "%s1234", softap_ssid);

    ESP_LOGI(TAG, "Generated SSID: %s", softap_ssid);
    ESP_LOGI(TAG, "Generated Password: %s", softap_password);
}

// Function to initialize NVS
void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Initializing NVS ...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    } else {
        ESP_LOGW(TAG, "NVS already initialized");
    }
    ESP_ERROR_CHECK(ret);
}

// Function to handle Wi-Fi and IP events
static void wifi_provisioning_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_START) {
        ESP_LOGI(TAG, "Provisioning started");
    } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV) {
        wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        ESP_LOGI(TAG, "Received Wi-Fi credentials - SSID: %s, Password: %s",
                 (const char *) wifi_sta_cfg->ssid,
                 (const char *) wifi_sta_cfg->password);
    } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END) {
        wifi_prov_mgr_deinit();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {  // Corrected event
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from Wi-Fi network, reconnecting...");
        esp_wifi_connect();
    }
}

// initialize wifi
void initialize_wifi() {
   // Initialize Wi-Fi in station mode
    ESP_ERROR_CHECK(esp_netif_init());

    // Register event handlers for Wi-Fi and Provisioning
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_provisioning_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_provisioning_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_provisioning_event_handler, NULL));  // Corrected registration

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

// Function to start Wi-Fi
void start_wifi(bool provisioned) {
    if (!provisioned) {
        esp_netif_create_default_wifi_ap();

        generate_softap_credentials();

        ESP_LOGI(TAG, "Starting provisioning");

        // Start Wi-Fi provisioning with SoftAP mode
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL, softap_ssid, softap_password));
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi");

        // Start Wi-Fi with stored credentials
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}
