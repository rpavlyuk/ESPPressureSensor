
# CA Certificate Manager for ESP-IDF

The **CA Certificate Manager** component provides a reliable way to manage Certificate Authority (CA) certificates in ESP-IDF projects. It enables storing certificates securely in Non-Volatile Storage (NVS) and optionally syncing them to the filesystem for broader use in secure communication, such as HTTPS or MQTT.

Load order: NVS -> (if not found) -> SPIFFS -> (if found) -> [save to NVS]
Save order: NVS -> (if requested) -> SPIFFS

## Features

- Initiate certificates from SPIFFS
- Store and retrieve CA certificates from NVS.
- Sync certificates to SPIFFS if required.
- Ensure certificates are easily accessible for secure connections.

## Dependencies

This component requires the following:

- **ESP-IDF** for base functionality.
- **NVS (Non-Volatile Storage)** for certificate storage.
- **ESP32_NVS_LARGE** for handling large certificate strings in NVS.

Ensure your project includes these modules.

> [!NOTE]
> Ensure NVS partition size is at least 64kB (recommended 128kB+) so that the manager works correctly.

## Functions
* Save certificate to NVS (and optionally -- to SPIFFS):
```c
/**
 * @brief: Save the CA certificate to the filesystem.
 * 
 * This function saves the CA certificate to NVS using large string NVS library (ESP32_NVS_LARGE).
 * If the certificate is successfully saved to NVS, it will also save the certificate to the filesystem if
 * the flag DO_SYNC_CA_CERT_TO_SPIFFS is set to true.
 * 
 * @note NVS and SPIFFS storage engines shall be initialized before calling this function.
 * 
 * @param[in] ca_cert The CA certificate to save.
 * @param[in] ca_cert_path The path to save the CA certificate.
 * @param[in] create_if_not_exist Flag to create the file if it does not exist.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the certificate cannot be saved.
 */
esp_err_t save_ca_certificate(const char *ca_cert, const char *ca_cert_path, bool create_if_not_exist);
```
* Load certificate. If found in NVS -- it will be loaded from there, if not -- then from SPIFFS and then synced into NVS:
```c
/**
 * @brief: Save the CA certificate to the filesystem.
 * 
 * This function saves the CA certificate to NVS using large string NVS library (ESP32_NVS_LARGE).
 * If the certificate is successfully saved to NVS, it will also save the certificate to the filesystem if
 * the flag DO_SYNC_CA_CERT_TO_SPIFFS is set to true.
 * 
 * @note NVS and SPIFFS storage engines shall be initialized before calling this function.
 * 
 * @param[in] ca_cert The CA certificate to save.
 * @param[in] ca_cert_path The path to save the CA certificate.
 * @param[in] create_if_not_exist Flag to create the file if it does not exist.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the certificate cannot be saved.
 */
esp_err_t save_ca_certificate(const char *ca_cert, const char *ca_cert_path, bool create_if_not_exist);
```

## Usage Example

Below is an example of how to use the CA Certificate Manager in your ESP32 project.

### 1. Init NVS and SPIFFS

Functions require both NVS and SPIFFS to be intialized before you can start using them.

```c
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "My Program";

// You can also use that same function from ESP32_NVS library
esp_err_t nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

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

void app_main(void) {

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_init());

    // Initialize FS
    init_filesystem();

    <your code here>
}

```

### 2. Include the Header

Include the CA Certificate Manager in your source file:

```c
#include "ca_cert_manager.h"
```

### 3. Save a CA Certificate

To save a CA certificate to NVS and optionally sync it to SPIFFS:

```c
const char *ca_cert = "-----BEGIN CERTIFICATE-----\n...certificate data...\n-----END CERTIFICATE-----";
esp_err_t err = save_ca_certificate(ca_cert, "/spiffs/my_ca_cert.pem", true);
if (err == ESP_OK) {
    printf("CA certificate saved successfully.\n");
} else {
    printf("Failed to save CA certificate: %s\n", esp_err_to_name(err));
}
```

### 4. Load a CA Certificate

To load a CA certificate from NVS or SPIFFS:

```c
char *loaded_cert = NULL;
err = load_ca_certificate(&loaded_cert, "/spiffs/my_ca_cert.pem");
if (err == ESP_OK) {
    printf("Loaded CA certificate:\n%s\n", loaded_cert);
    free(loaded_cert);
} else {
    printf("Failed to load CA certificate: %s\n", esp_err_to_name(err));
}
```

## Integration

Add the component to your component's `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES ca_cert_manager
)
```

## License

This project is licensed under the [MIT License](LICENSE).

Author: Roman Pavlyuk <roman@pavlyuk.consulting>
