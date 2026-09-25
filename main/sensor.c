#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "cJSON.h"

#include <math.h>

#include "common.h"
#include "util.h"
#include "flags.h"
#include "sensor.h"
#include "settings.h"
#include "mqtt.h"
#include "zigbee.h"
#include "non_volatile_storage.h"

sensor_data_t s_sensor_data;

static portMUX_TYPE s_data_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_running;
static bool s_stop_requested;


/**
 * @brief: Create a copy of global sensor_data variable
 */
sensor_data_t get_sensor_data() {
    sensor_data_t copy;
    portENTER_CRITICAL(&s_data_lock);
    copy = s_sensor_data;
    portEXIT_CRITICAL(&s_data_lock);
    return copy;
}

/**
 * @brief Stores the provided sensor data into the global sensor_data variable.
 * @param data Pointer to the sensor data to store.
 */
static void store_sensor_data(const sensor_data_t *data) {
    portENTER_CRITICAL(&s_data_lock);
    s_sensor_data = *data;
    portEXIT_CRITICAL(&s_data_lock);
}

/**
 * @brief Request the sensor task to stop.
 *
 * @note This function sets a flag indicating that the sensor task should stop.
 *       The actual stopping of the task should be handled within the task itself.
 */
void sensor_request_stop(void) {
    portENTER_CRITICAL(&s_data_lock);
    if (s_running) s_stop_requested = true;
    portEXIT_CRITICAL(&s_data_lock);
}

/**
 * @brief Checks if a stop has been requested for the sensor task.
 * @return true if a stop has been requested, false otherwise.
 */
static bool stop_requested(void) {
    bool stop;
    portENTER_CRITICAL(&s_data_lock);
    stop = s_stop_requested;
    portEXIT_CRITICAL(&s_data_lock);
    return stop;
}

/**
 * @brief Initializes the ADC calibration handle for the specified ADC unit and channel.
 * @param unit The ADC unit to initialize the calibration for.
 * @param channel The ADC channel to initialize the calibration for.
 * @param atten The ADC attenuation setting.
 * @param out_handle Pointer to the variable that will receive the initialized ADC calibration handle.
 * @return true if the calibration handle was successfully initialized, false otherwise.
 * @note The caller is responsible for providing a valid pointer for out_handle.
 * @see sensor_adc_calibration_deinit
 */
bool sensor_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    if (out_handle == NULL) return false;
    *out_handle = NULL;
    adc_cali_handle_t handle = NULL;
    esp_err_t err;
    /* Select the same scheme in init and deinit. No ambiguous cross-scheme fallback. */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t config = {
        .unit_id = unit, .chan = channel, .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_curve_fitting(&config, &handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    (void)channel;
    adc_cali_line_fitting_config_t config = {
        .unit_id = unit, .atten = atten, .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_line_fitting(&config, &handle);
#else
    (void)unit;
    (void)channel;
    (void)atten;
    err = ESP_ERR_NOT_SUPPORTED;
#endif
    if (err == ESP_OK && handle != NULL) {
        *out_handle = handle;
        ESP_LOGI(TAG, "ADC calibration initialized");
        return true;
    }
    if (err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "ADC calibration is not supported by this configuration/device");
    } else {
        ESP_LOGE(TAG, "ADC calibration failed: %s (handle=%p)",
                 esp_err_to_name(err), (void *)handle);
    }
    return false;
}

/**
 * @brief Deinitializes the ADC calibration handle.
 * @param handle The ADC calibration handle to deinitialize.
 * 
 * @note This function should be called to clean up the calibration handle when it is no longer needed.
 * @see sensor_adc_calibration_init
 * @see sensor_adc_calibration_deinit
 */
void sensor_adc_calibration_deinit(adc_cali_handle_t handle)
{
    if (handle == NULL) return;
    esp_err_t err;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    err = adc_cali_delete_scheme_curve_fitting(handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    err = adc_cali_delete_scheme_line_fitting(handle);
#else
    err = ESP_ERR_NOT_SUPPORTED;
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC calibration cleanup failed: %s", esp_err_to_name(err));
    }
}
/**
 * @brief Entry point for the sensor task.
 * 
 * This function initializes the ADC and calibration handles, waits for the device to become ready,
 * and then enters the main sensor reading and publishing loop.
 * 
 * @param pvParameters Task parameters (unused).
 */
void sensor_run(void *pvParameters)
{
    // Task entry point for the sensor reading and publishing loop
    (void)pvParameters;
    portENTER_CRITICAL(&s_data_lock);
    bool already_running = s_running;
    if (!already_running) { s_running = true; s_stop_requested = false; }
    portEXIT_CRITICAL(&s_data_lock);
    if (already_running) {
        ESP_LOGE(TAG, "Sensor task already running");
        vTaskDelete(NULL);
        return;
    }

    // Initialize ADC and calibration handles
    adc_oneshot_unit_handle_t adc = NULL;
    adc_cali_handle_t calibration = NULL;
    int *samples = NULL;
    bool startup_failed = false;
    const size_t capacity = SENSOR_SAMPLING_COUNT_MAX;
    const TickType_t readiness_wait = pdMS_TO_TICKS(SENSOR_WAIT_READY_MS);
    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "Waiting for device to become ready (timeout: %d s)", SENSOR_WAIT_READY_MS / 1000);
    if (g_sys_events == NULL) {
        ESP_LOGE(TAG, "System event group unavailable");
        startup_failed = true;
        goto cleanup;
    }
    EventBits_t bits = xEventGroupWaitBits(g_sys_events, BIT_DEVICE_READY,
                                         pdFALSE, pdTRUE, readiness_wait);
    if (stop_requested()) goto cleanup;
    if (!(bits & BIT_DEVICE_READY)) {
        ESP_LOGE(TAG, "Device did not become ready within %d seconds", SENSOR_WAIT_READY_MS / 1000);
        startup_failed = true;
        goto cleanup;
    } else {
        ESP_LOGI(TAG, "Sensor device is ready");
    }

    // Initialize ADC and calibration for the sensor
    ESP_LOGI(TAG, "Initializing ADC and calibration for the sensor");
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    err = adc_oneshot_new_unit(&unit_config, &adc);
    if (err != ESP_OK) { startup_failed = true; goto cleanup; }
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN, .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(adc, PRESSURE_SENSOR_PIN, &channel_config);
    if (err != ESP_OK) { startup_failed = true; goto cleanup; }
    if (!sensor_adc_calibration_init(ADC_UNIT_1, PRESSURE_SENSOR_PIN,
                                     ADC_ATTEN, &calibration)) {
        startup_failed = true;
        goto cleanup;
    }
    sensor_data_t initial = {0};
    store_sensor_data(&initial);

    while (!stop_requested()) {
        uint16_t interval = S_DEFAULT_SENSOR_READ_INTERVAL;
        err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &interval);
        if (err != ESP_OK || interval == 0) {
            ESP_LOGW(TAG, "Invalid/unavailable measurement interval; using %d ms", S_DEFAULT_SENSOR_READ_INTERVAL);
            interval = S_DEFAULT_SENSOR_READ_INTERVAL;
        }
        sensor_data_t next = {0};
        uint16_t sampling = S_DEFAULT_SENSOR_SAMPLING_ENABLE;
        err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, &sampling);
        if (err != ESP_OK) goto cycle_done;

        // Read voltage from the sensor and perform sampling if enabled
        if (sampling) {
            /* Allocate at most once per task lifetime; reuse across every cycle. */
            if (samples == NULL) {
                if (capacity == 0 || capacity > SIZE_MAX / sizeof(*samples)) {
                    err = ESP_ERR_INVALID_SIZE;
                    goto cycle_done;
                }
                samples = malloc(capacity * sizeof(*samples));
                if (samples == NULL) { err = ESP_ERR_NO_MEM; goto cycle_done; }
            }
            float mv = sample_voltage(calibration, adc, PRESSURE_SENSOR_PIN,
                                      samples, capacity, &next.voltage_raw, true);
            if (!isfinite(mv)) { err = ESP_FAIL; goto cycle_done; }
            next.voltage = mv / 1000.0f;
        } else {
            int mv = 0;
            err = adc_oneshot_read(adc, PRESSURE_SENSOR_PIN, &next.voltage_raw);
            if (err == ESP_OK)
                err = adc_cali_raw_to_voltage(calibration, next.voltage_raw, &mv);
            if (err != ESP_OK) goto cycle_done;
            next.voltage = mv / 1000.0f;
        }

        // Convert voltage to pressure using the offset and multiplier from NVS
        err = nvs_read_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, &next.voltage_offset);
        if (err == ESP_OK)
            err = nvs_read_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER,
                                  &next.sensor_linear_multiplier);
        if (err != ESP_OK) goto cycle_done;
        next.pressure = (next.voltage - next.voltage_offset) * next.sensor_linear_multiplier;
        if (!isfinite(next.voltage_offset) || !isfinite(next.pressure)) {
            err = ESP_ERR_INVALID_ARG;
            goto cycle_done;
        }
        if (stop_requested()) break;
        store_sensor_data(&next);
        ESP_LOGI(TAG, "ADC counts: %d, Voltage: %.4f V, Pressure: %.2f Pa",
                 next.voltage_raw, next.voltage, next.pressure);
        bits = xEventGroupWaitBits(g_sys_events, BIT_WIFI_CONNECTED,
                                  pdFALSE, pdTRUE, readiness_wait);
        if (stop_requested()) break;

        // Wait for Wi-Fi connection before attempting to publish sensor data
        if (bits & BIT_WIFI_CONNECTED) {
            /* REQUIRED integration: trigger must copy *next before returning.
             * See INTEGRATION.md. Never queue this pointer itself. */
            err = trigger_mqtt_publish(&next);
        }
cycle_done:
        if (err != ESP_OK && !stop_requested()) {
            ESP_LOGW(TAG, "Sensor cycle skipped/failed: %s", esp_err_to_name(err));
        }
        if (!stop_requested()) vTaskDelay(nonzero_ticks(interval));
    }

cleanup:
    if (err != ESP_OK) ESP_LOGE(TAG, "Sensor task error: %s", esp_err_to_name(err));
    free(samples);
    sensor_adc_calibration_deinit(calibration);
    if (adc != NULL) {
        esp_err_t cleanup_err = adc_oneshot_del_unit(adc);
        if (cleanup_err != ESP_OK)
            ESP_LOGE(TAG, "ADC cleanup failed: %s", esp_err_to_name(cleanup_err));
    }
    portENTER_CRITICAL(&s_data_lock);
    s_running = false;
    s_stop_requested = false;
    portEXIT_CRITICAL(&s_data_lock);
#if REBOOT_ON_SENSOR_FAILURE
    if (startup_failed) esp_restart();
#else
    (void)startup_failed;
#endif
    vTaskDelete(NULL);
}

/**
 * @brief Cleans up the ADC and calibration handles.
 * This function deinitializes the ADC calibration handle and the ADC unit handle, freeing any associated resources. It should be called when the sensor task is stopping or when the ADC is no longer needed.
 * @param calibration The ADC calibration handle to deinitialize.
 * @param adc The ADC unit handle to deinitialize.
 * 
 * @note This function should be called before freeing the ADC and calibration handles to ensure proper cleanup.
 * @see adc_oneshot_del_unit
 * @see sensor_adc_calibration_deinit
 */
static float sample_voltage(adc_cali_handle_t calibration,
                            adc_oneshot_unit_handle_t adc, adc_channel_t channel,
                            int *samples, size_t capacity, int *raw_mean,
                            bool cancellable)
{
    if (calibration == NULL || adc == NULL || samples == NULL) return NAN;
    uint16_t count = S_DEFAULT_SENSOR_SAMPLING_COUNT;
    uint16_t interval = S_DEFAULT_SENSOR_SAMPLING_INTERVAL;
    uint16_t deviation = S_DEFAULT_SENSOR_SAMPLING_MEDIAN_DEVIATION;
    esp_err_t err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &count);
    if (err == ESP_OK)
        err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &interval);
    if (err == ESP_OK)
        err = nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, &deviation);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sampling settings unavailable: %s", esp_err_to_name(err));
        return NAN;
    }
    if (count == 0 || count > SENSOR_SAMPLING_COUNT_MAX || count > capacity) {
        ESP_LOGE(TAG, "Invalid sampling count: %u", (unsigned)count);
        return NAN;
    }

    int64_t raw_sum = 0;
    for (int i = 0; i < count; ++i) {
        if (cancellable && stop_requested()) return NAN;
        int raw = 0;
        int mv = 0;
        err = adc_oneshot_read(adc, channel, &raw);
        if (err == ESP_OK) err = adc_cali_raw_to_voltage(calibration, raw, &mv);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ADC sample failed: %s", esp_err_to_name(err));
            return NAN;
        }
        raw_sum += raw;
        samples[i] = mv;
        vTaskDelay(nonzero_ticks(interval));
    }
    int median = calculate_median(samples, count);
    float allowed = fabsf((float)median) * deviation / 100.0f;
    float sum = 0.0f;
    int accepted = 0;
    for (int i = 0; i < count; ++i) {
        if (fabsf((float)samples[i] - (float)median) <= allowed) {
            sum += samples[i];
            ++accepted;
        }
    }
    if (raw_mean != NULL) *raw_mean = (int)(raw_sum / count);
    return accepted > 0 ? sum / accepted : (float)median;
}

/**
 * @brief Performs smart sampling and calculates the average voltage.
 * @param calibration The ADC calibration handle.
 * @param adc The ADC unit handle.
 * @param channel The ADC channel to sample.
 * @param calibrated Indicates whether the ADC is calibrated.
 * @return The average voltage in millivolts, or NAN on error.
 */
float perform_smart_sampling(adc_cali_handle_t calibration, adc_oneshot_unit_handle_t adc, adc_channel_t channel, bool calibrated) {
     if (!calibrated || calibration == NULL || adc == NULL) return NAN;
    size_t capacity = SENSOR_SAMPLING_COUNT_MAX;
    if (capacity == 0 || capacity > SIZE_MAX / sizeof(int)) return NAN;
    int *samples = malloc(capacity * sizeof(*samples));
    if (samples == NULL) return NAN;
    float mv = sample_voltage(calibration, adc, channel, samples, capacity, NULL, false);
    free(samples);
    return mv;
}

/**
 * @brief Converts a sensor_data_t structure to a cJSON object.
 * @param data The sensor data structure to convert to JSON.
 * @return A cJSON object representing the sensor state, or NULL on error.  
 */
cJSON *sensor_state_to_JSON(sensor_data_t *data) {
    if (data == NULL || !isfinite(data->pressure) || !isfinite(data->voltage)
        || !isfinite(data->voltage_offset)) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    if (cJSON_AddNumberToObject(root, "pressure", round((double)data->pressure * 100.0) / 100.0) == NULL
        || cJSON_AddNumberToObject(root, "voltage", round((double)data->voltage * 10000.0) / 10000.0) == NULL
        || cJSON_AddNumberToObject(root, "voltage_offset", round((double)data->voltage_offset * 1000.0) / 1000.0) == NULL
        || cJSON_AddNumberToObject(root, "sensor_linear_multiplier", data->sensor_linear_multiplier) == NULL
        || cJSON_AddNumberToObject(root, "voltage_raw", data->voltage_raw) == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

/**
 * @brief Converts a sensor_status_t structure to a cJSON object.
 * @param status The sensor status structure to convert to JSON.
 * @return A cJSON object representing the sensor status, or NULL on error.
 */
cJSON *sensor_status_to_JSON(sensor_status_t *status) {
    if (status == NULL) return NULL;
    uint16_t sampling = S_DEFAULT_SENSOR_SAMPLING_ENABLE;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_ENABLE, &sampling) != ESP_OK)
        return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    if (cJSON_AddNumberToObject(root, "free_heap", status->free_heap) == NULL
        || cJSON_AddNumberToObject(root, "min_free_heap", status->min_free_heap) == NULL
        || cJSON_AddNumberToObject(root, "time_since_boot", status->time_since_boot) == NULL
#if _DEVICE_ENABLE_STATUS_MEMGUARD
        || cJSON_AddNumberToObject(root, "memguard_threshold", status->memguard_threshold) == NULL
        || cJSON_AddNumberToObject(root, "memguard_mode", status->memguard_mode) == NULL
#endif
        || cJSON_AddNumberToObject(root, "sensor_sampling_enabled", sampling) == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

/**
 * @brief Serializes sensor state information to a JSON string.
 * @param data The sensor data structure to serialize.
 * @return A JSON string representing the sensor state, or NULL on error.
 */
char *serialize_sensor_state(sensor_data_t *data){

    return print_and_delete(sensor_state_to_JSON(data));
}

/**
 * @brief Serializes sensor status information to a JSON string.
 * @param status The sensor status structure to serialize.
 * @return A JSON string representing the sensor status, or NULL on error.
 */
char *serialize_sensor_status(sensor_status_t *status) {

    return print_and_delete(sensor_status_to_JSON(status));

}

/**
 * @brief Serializes sensor state and status information to a JSON string.
 * @param status The sensor status structure to serialize.
 * @param data The sensor data structure to serialize.
 * @return A JSON string representing all device data, or NULL on error.
 */
char *serialize_all_device_data(sensor_status_t *status, sensor_data_t *data)
{
    return print_and_delete(sensor_all_to_JSON(status, data));
}

/**
 * @brief Compiles a JSON object from sensor state and device status information.
 * @param status The sensor status structure to include in the JSON object.
 * @param sensor The sensor data structure to include in the JSON object.
 * @return A cJSON object representing all device data, or NULL on error.
 */
cJSON *sensor_all_to_JSON(sensor_status_t *status, sensor_data_t *sensor) {

    if (status == NULL || sensor == NULL) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    if (!attach_owned(root, "sensor", sensor_state_to_JSON(sensor))
        || !attach_owned(root, "status", sensor_status_to_JSON(status))) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;

}
