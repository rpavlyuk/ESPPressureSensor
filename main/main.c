#include <stdio.h>
#include "esp_log.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#include "main.h"
#include "wifi.h"
#include "sensor.h"

void app_main(void) {
    
    // Initialize NVS
    init_nvs();

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


    // run the sensor
    sensor_run();
}
