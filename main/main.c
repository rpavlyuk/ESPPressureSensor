#include <stdio.h>
#include "esp_log.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#include "non_volatile_storage.h"

#include "main.h"
#include "settings.h"
#include "wifi.h"
#include "sensor.h"
#include "web.h"

void app_main(void) {
    
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_init());

    // Init settings
    ESP_ERROR_CHECK(settings_init()); 

    /* Warm welcome in the console */
    char *device_id;
    char *device_serial;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_LOGI(TAG, "*** Starting ESP32-based Pressure Sonsor device ***");
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    ESP_LOGI(TAG, "Device Serial: %s", device_serial);

    // init internal filesystem
    init_filesystem();

    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize WIFI */
    initialize_wifi();

    // Initialize Wi-Fi provisioning manager
    wifi_prov_mgr_config_t wifi_config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(wifi_config));
    ESP_LOGI(TAG, "WiFi provisioning manager initialization complete");

    // Check if the device is already provisioned
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    // Initialize and start Wi-Fi
    start_wifi(provisioned);

    // start web
    start_webserver();

    // run the sensor
    sensor_run();
}
