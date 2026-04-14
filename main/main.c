#include "freertos/FreeRTOS.h"   // must be first
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"      // if you use queues

#include "common.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_softap.h"
#else
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#endif

#include "non_volatile_storage.h"

#include "main.h"
#include "settings.h"
#include "flags.h"
#include "wifi.h"
#include "sensor.h"
#include "web.h"
#if _DEVICE_ENABLE_MQTT
#include "mqtt.h"
#endif
#if _DEVICE_ENABLE_ZIGBEE
#include "zigbee.h"
#endif
#if _DEVICE_ENABLE_STATUS
#include "status.h"
#endif

EventGroupHandle_t g_sys_events;

void app_main(void) {

    bool wifi_provisioned = false;

    /* Create the system events group */
    g_sys_events = xEventGroupCreate();
    if (g_sys_events == NULL) {
        ESP_LOGE(TAG, "Failed to create system event group");
        return; // or handle error gracefully
    }

    /* Reset system event bits */
    reset_system_bits();

    /* Logging setup */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("non_volatile_storage", ESP_LOG_WARN);
    esp_log_level_set("NVS_LARGE", ESP_LOG_VERBOSE);
 
    // Check partition table to ensure consistancy after OTA update
    ESP_ERROR_CHECK(check_ota_partitions());

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_init());

    // Init settings
    ESP_ERROR_CHECK(settings_init());

    // init internal filesystem required for web server
    init_filesystem();

    /* Warm welcome in the console */
    char *device_id;
    char *device_serial;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_LOGI(TAG, "*** Starting ESP32-based Pressure Sensor device ***");
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    ESP_LOGI(TAG, "Device Serial: %s", device_serial);

    // Free allocated memory after usage
    free(device_id);
    free(device_serial);

    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if _DEVICE_ENABLE_WIFI
    ESP_LOGI(TAG, "WIFI ENABLED!");

    /* Initialize WIFI */
    initialize_wifi();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    // Initialize network provisioning manager
    network_prov_mgr_config_t wifi_config = {
        .scheme = network_prov_scheme_softap,
        .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE
    };
    ESP_ERROR_CHECK(network_prov_mgr_init(wifi_config));
    ESP_LOGI(TAG, "Network provisioning manager initialization complete");

    // Check if the device is already provisioned
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&wifi_provisioned));
#else
    // Initialize Wi-Fi provisioning manager
    wifi_prov_mgr_config_t wifi_config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(wifi_config));
    ESP_LOGI(TAG, "WiFi provisioning manager initialization complete");

    // Check if the device is already provisioned
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&wifi_provisioned));
#endif

    // Initialize and start Wi-Fi
    start_wifi(wifi_provisioned);

    if (wifi_provisioned) {
        ESP_LOGI(TAG, "WiFi is provisioned!");

        xEventGroupSetBits(g_sys_events, BIT_WIFI_PROVISIONED);
        
        // Wifi is provisioned but might not be ready for now. Wait until Wi-Fi is ready
        EventBits_t bits = xEventGroupWaitBits(
            g_sys_events,
            BIT_WIFI_CONNECTED,
            pdFALSE,                // don't clear
            pdTRUE,
            pdMS_TO_TICKS(30000)    // wait up to 30 seconds
        );

        if (bits & BIT_WIFI_CONNECTED) {
            ESP_LOGI(TAG, "main: Wi-Fi/network is ready!");
        } else {
            ESP_LOGW(TAG, "main: Timeout waiting for Wi-Fi to connect");
            ESP_LOGW(TAG, "main: Wi-Fi/network never became ready");
            ESP_LOGW(TAG, "main: Sending ESP32 to reboot...");
            esp_restart();
            return;
        }

#if _DEVICE_ENABLE_WEB || _DEVICE_ENABLE_HTTP_API
        // start web server
        ESP_LOGI(TAG, "WEB and/or HTTP API ENABLED!");

        xTaskCreate(run_http_server, "run_http_server", 16384, NULL, 5, NULL);
#endif

#if _DEVICE_ENABLE_MQTT
        // get MQTT connection mode from NVS
        uint16_t mqtt_connection_mode;
        ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));

        // start MQTT client
        if (mqtt_connection_mode) {
            ESP_LOGI(TAG, "MQTT ENABLED by MODE setting (%d)!", mqtt_connection_mode);

            /* Start MQTT publishing queue */
            ESP_ERROR_CHECK(start_mqtt_queue_task());

            // init MQTT connection
            if (mqtt_init() == ESP_OK) {
                ESP_LOGI(TAG, "Connected to MQTT server!");
            } else {
                ESP_LOGE(TAG, "Unable to connect to MQTT broker");
                return;
            }

#if _DEVICE_ENABLE_HA
            ESP_LOGI(TAG, "HA device status ENABLED!");
            xTaskCreate(mqtt_device_config_task, "mqtt_device_config_task", 4096, NULL, 5, NULL);
#endif
        }

#endif

    }
#endif

#if _DEVICE_ENABLE_ZIGBEE
// raise compiler error if both WiFi and Zigbee are enabled to avoid potential instability issues
#if _DEVICE_ENABLE_WIFI
#error "Both WiFi and Zigbee are enabled. Please disable one of them in main/common.h to avoid potential instability issues."
#endif

    ESP_LOGI(TAG, "Zigbee ENABLED!");
    // run zigbee
    ESP_ERROR_CHECK(zigbee_init());
#endif

#if _DEVICE_ENABLE_STATUS
        ESP_LOGI(TAG, "Status ENABLED!");
        // Start the status monitoring task 
        status_init();
#endif

    // run the sensor
    xTaskCreate(sensor_run, "sensor_task", 8192, NULL, 5, NULL);
}
