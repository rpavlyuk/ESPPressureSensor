#include "freertos/FreeRTOS.h"   // must be first
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"      // if you use queues

#include "esp_idf_version.h"

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_softap.h"
#else
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#endif
#include "qrcode.h"
#include "cJSON.h"
#include "esp_event.h"

#include "wifi.h"
#include "flags.h"

char softap_ssid[32];       // Buffer for the generated SSID
char softap_password[64];   // Buffer for the generated password

esp_netif_t *esp_netif_sta;


/**
 * @brief Convert Wi-Fi disconnect reason code to human-readable string
 * Note: This is a best-effort mapping based on known reason codes. Some codes may not be covered.
 * Refer to esp_wifi.h and Wi-Fi specifications for more details on reason codes.
 * 
 * @param r The Wi-Fi disconnect reason code
 * @return const char* A string representation of the reason code
 */
static const char *wifi_disc_reason_to_str(uint8_t r)
{
    switch (r) {
    case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
#else
    case WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA: return "CLASS2_FRAME_FROM_NONAUTH_STA";
    case WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA: return "CLASS3_FRAME_FROM_NONASSOC_STA";
#endif

    case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
#endif

    case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_IE_INVALID: return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    default: return "UNKNOWN";
    }
}

/**
 * @brief Print the provisioning QR code for SoftAP provisioning
 * This function generates a JSON payload containing the provisioning information (version, service name, proof of possession, transport type, and network type) and prints it as a QR code in the console. The QR code can be scanned using the ESP Provisioning app to facilitate the provisioning process. The function also logs a URL that can be used to view the QR code if it's not visible in the console. This is useful for debugging and ensuring that the correct provisioning information is being generated.
 * @param service_name The name of the provisioning service (e.g., "ESP32_Provisioning")
 * @param pop The proof of possession (POP) string that is required for secure provisioning. This should match the POP configured in the provisioning app.
 * @return void
 */
static void print_prov_qr_softap(const char *service_name, const char *pop)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create QR JSON");
        return;
    }

    cJSON_AddStringToObject(root, "ver", "v1");
    cJSON_AddStringToObject(root, "name", service_name);
    cJSON_AddStringToObject(root, "pop", pop);
    cJSON_AddStringToObject(root, "transport", "softap");
    cJSON_AddStringToObject(root, "network", "wifi");

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to serialize QR JSON");
        return;
    }

    ESP_LOGI(TAG, "Scan this QR code from the ESP Provisioning app:");
    ESP_LOGI(TAG, "If QR is not visible, open:");
    ESP_LOGI(TAG, "https://espressif.github.io/esp-jumpstart/qrcode.html?data=%s", payload);
    ESP_LOGI(TAG, "OR manually connect to the SSID '%s' with password '%s' on your smartphone device running the 'ESP SoftAP Prov' app, if you're experiencing issues when scanning the QR code", service_name, pop);

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);

    free(payload);
}

/**
 * @brief Generate unique SoftAP credentials based on the device's MAC address
 * The SSID is generated in the format "PROV_AP_XXXXXX" where XXXXXX are the last 3 bytes of the MAC address in hexadecimal.
 * The password is generated by appending "1234" to the SSID (e.g., "PROV_AP_XXXXXX1234"). This ensures that each device has a unique SSID and password for provisioning, while keeping the credentials simple and consistent. The generated credentials are logged for debugging purposes.
 * TODO: In a production environment, consider using a more secure password generation strategy and avoid logging sensitive information.
 */
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

/**
 * @brief Log the currently configured STA credentials stored in NVS (Wi-Fi driver config)
 * This function retrieves the Wi-Fi configuration for the station interface (WIFI_IF_STA) using esp_wifi_get_config and logs the SSID and whether a password is set. It also logs the authentication mode and PMF capabilities. Note that the actual password is not logged for security reasons; instead, it indicates whether a password is set or if it's an open network. This function is useful for debugging purposes to verify what credentials are currently stored in the Wi-Fi driver before attempting to connect. In a production environment, be cautious about logging sensitive information such as Wi-Fi credentials.
 * @return esp_err_t Returns ESP_OK on success, or an error code if the Wi-Fi configuration cannot be retrieved.
 */
esp_err_t log_sta_credentials(void) {
    wifi_config_t cfg = {0};
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_get_config(WIFI_IF_STA) failed: %s", esp_err_to_name(err));
        return err;
    }

    // SSID/pass are not guaranteed to be null-terminated in all cases; make safe copies
    char ssid[33] = {0};      // 32 + NUL
    char pass[65] = {0};      // 64 + NUL

    memcpy(ssid, cfg.sta.ssid, sizeof(cfg.sta.ssid));
    memcpy(pass, cfg.sta.password, sizeof(cfg.sta.password));
    ssid[32] = '\0';
    pass[64] = '\0';

    // ⚠️ Strong recommendation: DO NOT log the password in production.
    ESP_LOGI(TAG, "Connecting to SSID '%s'%s", ssid, pass[0] ? " (password set)" : " (open network)");
    ESP_LOGI(TAG, "STA cfg: authmode=%d pmf_cap=%d pmf_req=%d",
         cfg.sta.threshold.authmode,
         cfg.sta.pmf_cfg.capable,
         cfg.sta.pmf_cfg.required);
    // If you *really* want it for debugging, gate it behind a debug flag:
    // ESP_LOGI(TAG, "Connecting to SSID '%s' with key '%s'", ssid, pass);

    return ESP_OK;
}


#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
/**
 * @brief Wi-Fi provisioning event handler for ESP-IDF 6.0 and above
 * This function handles various Wi-Fi provisioning events, such as start, credentials received, and end of provisioning.
 * It also handles Wi-Fi events like STA start, STA disconnected, and IP events like STA got IP.
 * The event handler updates the system event bits accordingly and logs relevant information for debugging purposes. In case of provisioning completion, it deinitializes the provisioning manager and restarts the device to apply the new Wi-Fi configuration.
 * @param arg User-defined argument (not used in this handler)
 * @param event_base The base of the event (e.g., NETWORK_PROV_EVENT, WIFI_EVENT, IP_EVENT)
 * @param event_id The ID of the event (e.g., NETWORK_PROV_START, WIFI_EVENT_STA_DISCONNECTED)
 * @param event_data Pointer to the event data structure, which varies based on the event type (e.g., wifi_sta_config_t for credentials
 * received, wifi_event_sta_disconnected_t for STA disconnected, ip_event_got_ip_t for got IP)
 * @return void
 */
static void wifi_provisioning_event_handler(void *arg,
                                            esp_event_base_t event_base,
                                            int32_t event_id,
                                            void *event_data)
{
    (void)arg;

    if (event_base == NETWORK_PROV_EVENT) {
        if (event_id == NETWORK_PROV_START) {
            ESP_LOGI(TAG, "Provisioning started");
            return;
        }

        if (event_id == NETWORK_PROV_WIFI_CRED_RECV) {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;

            if (wifi_sta_cfg != NULL) {
                ESP_LOGI(TAG, "Received Wi-Fi credentials - SSID: %s, Password: %s",
                         (const char *)wifi_sta_cfg->ssid,
                         (const char *)wifi_sta_cfg->password);
            } else {
                ESP_LOGW(TAG, "Provisioning credentials event received with NULL data");
            }
            return;
        }

        if (event_id == NETWORK_PROV_END) {
            network_prov_mgr_deinit();
            ESP_LOGI(TAG, "Wi-Fi Provisioning completed. Restarting the device now.");
            esp_restart();
            return;
        }
    }

    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi STA started, connecting...");
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "esp_wifi_connect() failed: %s", esp_err_to_name(err));
            }
            return;
        }

        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)event_data;

            xEventGroupClearBits(g_sys_events, BIT_WIFI_CONNECTED);
            xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED);
            xEventGroupClearBits(g_sys_events, BIT_MQTT_READY);

            if (d != NULL) {
                ESP_LOGW(TAG, "STA disconnected: reason=%u (%s)",
                         d->reason,
                         wifi_disc_reason_to_str(d->reason));
                ESP_LOGW(TAG, "SSID: %.*s", d->ssid_len, (const char *)d->ssid);
            } else {
                ESP_LOGW(TAG, "STA disconnected: event data is NULL");
            }

            dump_sys_bits("WIFI_EVENT_STA_DISCONNECTED");

            vTaskDelay(pdMS_TO_TICKS(2000));
            ESP_LOGI(TAG, "Calling Wi-Fi reconnect logic...");
            ESP_ERROR_CHECK(esp_wifi_connect());
            return;
        }
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        xEventGroupSetBits(g_sys_events, BIT_WIFI_CONNECTED);

        if (event != NULL) {
            log_network_configuration(event->esp_netif);
        } else {
            ESP_LOGW(TAG, "IP_EVENT_STA_GOT_IP received with NULL event data");
        }

        return;
    }
}
#else
/**
 * @brief Wi-Fi provisioning event handler for ESP-IDF versions below 6.0
 * This function handles various Wi-Fi provisioning events, such as start, credentials received, and end of provisioning.
 * It also handles Wi-Fi events like STA start, STA disconnected, and IP events like STA got IP.
 * The event handler updates the system event bits accordingly and logs relevant information for debugging purposes. In case of provisioning completion, it deinitializes the provisioning manager and restarts the device to apply the new Wi-Fi configuration.
 * @param arg User-defined argument (not used in this handler)
 * @param event_base The base of the event (e.g., WIFI_PROV_EVENT, WIFI_EVENT, IP_EVENT)
 * @param event_id The ID of the event (e.g., WIFI_PROV_START, WIFI_EVENT_STA_DISCONNECTED)
 * @param event_data Pointer to the event data structure, which varies based on the event type (e.g., wifi_sta_config_t for credentials
 * received, wifi_event_sta_disconnected_t for STA disconnected, ip_event_got_ip_t for got IP)
 * @return void
 */
static void wifi_provisioning_event_handler(void *arg,
                                            esp_event_base_t event_base,
                                            int32_t event_id,
                                            void *event_data)
{
    (void)arg;

    if (event_base == WIFI_PROV_EVENT) {
        if (event_id == WIFI_PROV_START) {
            ESP_LOGI(TAG, "Provisioning started");
            return;
        }

        if (event_id == WIFI_PROV_CRED_RECV) {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;

            if (wifi_sta_cfg != NULL) {
                ESP_LOGI(TAG, "Received Wi-Fi credentials - SSID: %s, Password: %s",
                         (const char *)wifi_sta_cfg->ssid,
                         (const char *)wifi_sta_cfg->password);
            } else {
                ESP_LOGW(TAG, "Provisioning credentials event received with NULL data");
            }
            return;
        }

        if (event_id == WIFI_PROV_END) {
            wifi_prov_mgr_deinit();
            ESP_LOGI(TAG, "Wi-Fi Provisioning completed. Restarting the device now.");
            esp_restart();
            return;
        }
    }

    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi STA started, connecting...");
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "esp_wifi_connect() failed: %s", esp_err_to_name(err));
            }
            return;
        }

        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)event_data;

            xEventGroupClearBits(g_sys_events, BIT_WIFI_CONNECTED);
            xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED);
            xEventGroupClearBits(g_sys_events, BIT_MQTT_READY);

            if (d != NULL) {
                ESP_LOGW(TAG, "STA disconnected: reason=%u (%s)",
                         d->reason,
                         wifi_disc_reason_to_str(d->reason));
                ESP_LOGW(TAG, "SSID: %.*s", d->ssid_len, (const char *)d->ssid);
            } else {
                ESP_LOGW(TAG, "STA disconnected: event data is NULL");
            }

            dump_sys_bits("WIFI_EVENT_STA_DISCONNECTED");

            vTaskDelay(pdMS_TO_TICKS(2000));
            ESP_LOGI(TAG, "Calling Wi-Fi reconnect logic...");
            ESP_ERROR_CHECK(esp_wifi_connect());
            return;
        }
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        xEventGroupSetBits(g_sys_events, BIT_WIFI_CONNECTED);

        if (event != NULL) {
            log_network_configuration(event->esp_netif);
        } else {
            ESP_LOGW(TAG, "IP_EVENT_STA_GOT_IP received with NULL event data");
        }

        return;
    }
}
#endif

/**
 * @brief Initialize Wi-Fi and register event handlers
 * This function initializes the TCP/IP stack, registers event handlers for Wi-Fi and provisioning events, creates the default Wi-Fi station network interface, and initializes the Wi-Fi driver with default configuration. It
 * must be called before starting Wi-Fi or provisioning. The event handlers will manage the Wi-Fi connection process and provisioning flow based on the events received. This function does not start the Wi-Fi; it only sets up the necessary components and handlers for Wi-Fi functionality.
 * @return void
 */
void initialize_wifi(void)
{
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Register event handlers for Wi-Fi and provisioning
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_provisioning_event_handler,
                                               NULL));
#else
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_provisioning_event_handler,
                                               NULL));
#endif

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_provisioning_event_handler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_provisioning_event_handler,
                                               NULL));

    esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

/**
 * @brief Start Wi-Fi in either provisioning mode (SoftAP) or normal STA mode based on whether the device is already provisioned
 * If the device is not provisioned, it starts Wi-Fi in SoftAP mode with generated credentials for provisioning. If the device is already provisioned, it starts Wi-Fi in STA mode and
 * logs the stored STA credentials for debugging purposes. It also sets a static DNS server (Google's 8.8.8.8).
 * This function should be called after initialize_wifi() and after checking the provisioning status. The event handlers registered in initialize_wifi() will handle the connection process and provisioning flow based on the events received.
 * @param provisioned A boolean indicating whether the device is already provisioned with Wi-Fi credentials. If false, the device will start in SoftAP provisioning mode. If true, it will start in STA mode and attempt to connect using stored credentials.
 * @return void
 */
void start_wifi(bool provisioned)
{
    if (!provisioned) {
        esp_netif_create_default_wifi_ap();

        generate_softap_credentials();

        ESP_LOGI(TAG, "Starting provisioning");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        // Start network provisioning with SoftAP mode
        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_1,
                                                            NULL,
                                                            softap_ssid,
                                                            softap_password));
#else
        // Start Wi-Fi provisioning with SoftAP mode
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1,
                                                         NULL,
                                                         softap_ssid,
                                                         softap_password));
#endif
        print_prov_qr_softap(softap_ssid, softap_password);
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi");

        // Log the stored STA credentials
        ESP_ERROR_CHECK(log_sta_credentials());

        // Start Wi-Fi with stored credentials
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        ESP_ERROR_CHECK(esp_wifi_start());

        esp_netif_dns_info_t dns = {0};
        dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");  // Set Google's DNS server
        ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns));
    }
}


void log_network_configuration(esp_netif_t *esp_netif_sta) {
    esp_netif_ip_info_t ip_info;

    ESP_LOGI(TAG, "+---- WIFI Connection Information ----+");
    // Get IP information (IP, netmask, and gateway)
    if (esp_netif_get_ip_info(esp_netif_sta, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    } else {
        ESP_LOGE(TAG, "Failed to get IP information");
    }

    // get DNS
    esp_netif_dns_info_t dns_info;
    esp_err_t err = esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns_info);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "DNS IP: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    } else {
        ESP_LOGE(TAG, "Failed to retrieve DNS info: %s", esp_err_to_name(err));
    }

    // Get default route (i.e., the default gateway)
    esp_ip6_addr_t ip6_info;
    if (esp_netif_get_ip6_linklocal(esp_netif_sta, &ip6_info) == ESP_OK) {
        ESP_LOGI(TAG, "IPv6 Address: " IPV6STR, IPV62STR(ip6_info));
    } else {
        ESP_LOGE(TAG, "Failed to get IPv6 information");
    }
}
