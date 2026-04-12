
# ESP32_NVS_LARGE

The **ESP32_NVS_LARGE** component extends ESP-IDF's NVS (Non-Volatile Storage) functionality by enabling the storage and retrieval of large strings. This allows to overcome the limit of ~4kB imposed by NVS on single key-value pair. It splits data into manageable chunks that fit within the constraints of NVS, and reassembles them when needed.

## Features

- **Chunked Storage**: Automatically splits large strings into chunks to store in NVS.
- **Reassembly**: Retrieves and reassembles large strings seamlessly.
- **Metadata Management**: Maintains metadata for efficient chunk handling.
- **Utility Functions**: Provides helper functions for operations like substring extraction and string appending.

## Dependencies

This component requires the following ESP-IDF modules:

- [**nvs_flash**](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html): Base NVS functionality.
- [**ESP32_NVS**](https://github.com/VPavlusha/ESP32_NVS): Provides additional NVS key-value abstraction layer.

## Usage Example

### 1. Ensure that NVS is initialized

Functions require NVS to be intialized before you can start using them.

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

void app_main(void) {

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_init());

    <your code here>
}

```

### 2. Include the Header

Include the `nvs_large.h` file in your source code:

```c
#include "nvs_large.h"
```

### 3. Save a Large String

To store a large string in NVS:

```c
const char *large_data = "This is a very large string exceeding NVS limits.";
esp_err_t err = nvs_write_string_large("storage_namespace", "large_key", large_data);
if (err == ESP_OK) {
    printf("Large string saved successfully.\n");
} else {
    printf("Failed to save large string: %s\n", esp_err_to_name(err));
}
```

### 3. Load a Large String

To retrieve a large string from NVS:

```c
char *retrieved_data = NULL;
err = nvs_read_string_large("storage_namespace", "large_key", &retrieved_data);
if (err == ESP_OK) {
    printf("Retrieved data: %s\n", retrieved_data);
    free(retrieved_data);
} else {
    printf("Failed to retrieve large string: %s\n", esp_err_to_name(err));
}
```

## Integration

Add the following to your `CMakeLists.txt` file to include this component:

```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES ESP32_NVS_LARGE
)
```

## License

This project is licensed under the [MIT License](LICENSE).
