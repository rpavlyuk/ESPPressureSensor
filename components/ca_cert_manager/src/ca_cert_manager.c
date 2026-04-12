#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "nvs_large.h"


#include "ca_cert_manager.h"

/**
 * @brief: Load the CA certificate from the filesystem.
 * 
 * This function loads the CA certificate from NVS using large string NVS library.
 * If the certificate is not found in NVS, it will attempt to load the certificate
 * from the filesystem. If successful, the certificate is saved to NVS for future use.
 * 
 * One certificate is saved to NVS, it will be read from NVS and SPIFFS storage will be ignored.
 * 
 * The certificate is used to verify the MQTT server's identity during the TLS handshake or
 * any other secure connection, e.g., HTTPS, especially during HTTPS OTA updates.
 * 
 * @note NVS and SPIFFS storage engines shall be initialized before calling this function.
 * 
 * @param[out] ca_cert The buffer to store the CA certificate.
 * @param[in] ca_cert_path The path to the CA certificate file.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the certificate cannot be loaded.
 */
esp_err_t load_ca_certificate(char **ca_cert, const char *ca_cert_path) {
    if (ca_cert_path == NULL) {
        ESP_LOGE(CRT_MGR_TAG, "CA certificate path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char *cert_file = esp_basename(ca_cert_path);
    if (cert_file == NULL) {
        ESP_LOGE(CRT_MGR_TAG, "Failed to extract basename from CA certificate path: %s", ca_cert_path);
        return ESP_FAIL;
    }

    ESP_LOGI(CRT_MGR_TAG, "CA certificate basename to be used as NVS key: %s", cert_file);

    // Attempt to load the certificate from NVS
    if (nvs_read_string_large(S_NAMESPACE, cert_file, ca_cert) == ESP_OK) {
        ESP_LOGI(CRT_MGR_TAG, "CA certificate loaded from NVS. SPIFFS storage will be ignored.");
        return ESP_OK;
    } else {
        ESP_LOGW(CRT_MGR_TAG, "Unable to load CA certificate from NVS. Proceeding with SPIFFS storage and further sync with NVS.");
    }

    // If loading from NVS fails, proceed to load from SPIFFS and sync to NVS
    FILE *f = fopen(ca_cert_path, "r");
    if (f == NULL) {
        ESP_LOGE(CRT_MGR_TAG, "Failed to open CA certificate file at path: %s", ca_cert_path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long cert_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (cert_size <= 0) {
        ESP_LOGE(CRT_MGR_TAG, "Invalid CA certificate file size");
        fclose(f);
        return ESP_FAIL;
    }

    *ca_cert = (char *)malloc(cert_size + 1);
    if (*ca_cert == NULL) {
        ESP_LOGE(CRT_MGR_TAG, "Failed to allocate memory for CA certificate");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(*ca_cert, 1, cert_size, f);
    (*ca_cert)[cert_size] = '\0';  // Null-terminate the string
    fclose(f);

    ESP_LOGI(CRT_MGR_TAG, "Successfully loaded CA certificate from path: %s", ca_cert_path);

    // Save the loaded certificate to NVS for future use
    esp_err_t err = nvs_write_string_large(S_NAMESPACE, cert_file, *ca_cert);
    if (err != ESP_OK) {
        ESP_LOGE(CRT_MGR_TAG, "Failed to save CA certificate to NVS: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

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
esp_err_t save_ca_certificate(const char *ca_cert, const char *ca_cert_path, bool create_if_not_exist)
{
    if (ca_cert == NULL) {
        ESP_LOGE(CRT_MGR_TAG, "CA certificate data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // get NVS key as the certificate filename
    char *cert_file = esp_basename(ca_cert_path);
    if(cert_file != NULL) {
        ESP_LOGI(CRT_MGR_TAG, "CA certificate basename to be used as NVS key: %s", cert_file);
    } else {
        ESP_LOGE(CRT_MGR_TAG, "Failed to extract basename from CA certificate path (%s). The path may be invlaid.", ca_cert_path);
        return ESP_FAIL;
    }

    // Save the certificate to NVS
    esp_err_t err = nvs_write_string_large(S_NAMESPACE, cert_file, ca_cert);
    if (err != ESP_OK) {
        ESP_LOGE(CRT_MGR_TAG, "Failed to save CA certificate to NVS. Error: %s", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        ESP_LOGI(CRT_MGR_TAG, "CA certificate (%s, %li bytes) saved to NVS", cert_file, (int32_t)strlen(ca_cert));
    }

    if (DO_SYNC_CA_CERT_TO_SPIFFS) {
        // Save the certificate to the filesystem
        FILE *f = fopen(ca_cert_path, create_if_not_exist ? "w+" : "w");
        if (f == NULL) {
            ESP_LOGE(CRT_MGR_TAG, "Failed to open CA certificate file for writing");
            return ESP_FAIL;
        }

        // Write the certificate to the file
        size_t written = fwrite(ca_cert, 1, strlen(ca_cert), f);
        if (written != strlen(ca_cert)) {
            ESP_LOGE(CRT_MGR_TAG, "Failed to write entire CA certificate to file");
            fclose(f);
            return ESP_FAIL;
        }

        fclose(f);
        ESP_LOGI(CRT_MGR_TAG, "Successfully saved CA certificate to file: %s", ca_cert_path);

    }

    return ESP_OK;
}

/**
 * @brief: Get base name of a file from its path
 * 
 * This function extracts the base name of a file from its path.
 * 
 * @param path: The file path
 * @return The base name of the file
 */
const char *esp_basename(const char *path) {
    if (path == NULL || *path == '\0') {
        return ".";
    }

    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }

    return path;
}
