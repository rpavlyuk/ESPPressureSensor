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
#include "flags.h"
#include "sensor.h"
#include "settings.h"
#include "mqtt.h"
#include "zigbee.h"
#include "non_volatile_storage.h"

sensor_data_t sensor_data;


/**
 * @brief: Create a copy of global sensor_data variable
 */
sensor_data_t get_sensor_data() {
    sensor_data_t s_data = {
        .pressure = sensor_data.pressure,
        .sensor_linear_multiplier = sensor_data.sensor_linear_multiplier,
        .voltage = sensor_data.voltage,
        .voltage_offset = sensor_data.voltage_offset,
        .voltage_raw = sensor_data.voltage_raw,
    };

    return s_data;
}
/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
bool sensor_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}


void sensor_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}


void sensor_run(void *pvParameters) {

    // wait for the device to become ready
        // Wait up to 30 seconds total
    int wait_time_ms = 30000;
    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,             // event group handle
        BIT_DEVICE_READY,       // bit(s) to wait for
        pdFALSE,                  // don't clear the bit on exit
        pdTRUE,                   // wait for all bits (only one here)
        pdMS_TO_TICKS(wait_time_ms)      // timeout 30 seconds
    );

    if ((bits & BIT_DEVICE_READY)) {
        ESP_LOGI(TAG, "%s: Device is ready!", __func__);
        // Continue normal operation
    } else {
        ESP_LOGE(TAG, "%s: Device never became ready after %d seconds", __func__, wait_time_ms/1000);
#if REBOOT_ON_SENSOR_FAILURE
        ESP_LOGE(TAG, "%s: Rebooting device due to sensor initialization failure", __func__);
        esp_restart();
#else
        ESP_LOGE(TAG, "%s: Sensor initialization failed. Halting sensor task.", __func__);
        vTaskDelete(NULL); // Delete the current task to halt execution of the sensor task
        return;
#endif
    }
    

    // Initialize ADC for the pressure sensor
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t adc_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, PRESSURE_SENSOR_PIN, &adc_config));


    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_pressure_sensor_handle = NULL;
    bool do_calibration1_pressure_sensor = sensor_adc_calibration_init(ADC_UNIT_1, PRESSURE_SENSOR_PIN, ADC_ATTEN, &adc1_cali_pressure_sensor_handle);
    

    ESP_LOGI(TAG, "Preparing sensor data structure");
    sensor_data.pressure = 0;

    ESP_LOGI(TAG, "Starting pressure sensing cycle");

    while (1) {
        // Read the raw sensor value from ADC
        sensor_data.voltage_raw = perform_smart_sampling(adc1_cali_pressure_sensor_handle, adc1_handle, PRESSURE_SENSOR_PIN, do_calibration1_pressure_sensor);

        // Obtain the voltage in Volts
        sensor_data.voltage = sensor_data.voltage_raw / 1000.0;

        // Calculate pressure in KPa using the provided formula
        ESP_ERROR_CHECK(nvs_read_float(S_NAMESPACE, S_KEY_SENSOR_OFFSET, &sensor_data.voltage_offset));
        ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_SENSOR_LINEAR_MULTIPLIER, &sensor_data.sensor_linear_multiplier));
        sensor_data.pressure = (sensor_data.voltage - sensor_data.voltage_offset) * sensor_data.sensor_linear_multiplier;  // Convert voltage to pressure in Pa

        // Print voltage and pressure to Serial Monitor
        ESP_LOGI(TAG, "Raw ADC Value: %d, Voltage: %.3f V, Pressure: %.2f Pa", 
                 sensor_data.voltage_raw, sensor_data.voltage, sensor_data.pressure);

        // Publish the sensor data via MQTT if it is ready
        bits = xEventGroupWaitBits(
            g_sys_events,             // event group handle
            BIT_WIFI_CONNECTED,       // bit(s) to wait for
            pdFALSE,                  // don't clear the bit on exit
            pdTRUE,                   // wait for all bits (only one here)
            pdMS_TO_TICKS(wait_time_ms)      // timeout 30 seconds
        );
        if (bits & BIT_WIFI_CONNECTED) {
            ESP_LOGI(TAG, "Wi-Fi is connected and provisioned. Proceeding to publish sensor data.");
            if (trigger_mqtt_publish(&sensor_data) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to trigger MQTT publish");
            }
        } else {
            ESP_LOGW(TAG, "Wi-Fi is not ready. Skipping MQTT publish for this cycle.");
        }

        uint16_t sensor_intervl = S_DEFAULT_SENSOR_READ_INTERVAL;
        ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_READ_INTERVAL, &sensor_intervl));
        ESP_LOGI(TAG, "Next pressure measurement cycle will start in %i seconds", (int) sensor_intervl / 1000);
        vTaskDelay(pdMS_TO_TICKS(sensor_intervl));
    }

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (do_calibration1_pressure_sensor) {
        sensor_adc_calibration_deinit(adc1_cali_pressure_sensor_handle);
    }
}

// Function to calculate the median of an array
int calculate_median(int* data, int size) {
    // Sort the data array
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (data[i] > data[j]) {
                int temp = data[i];
                data[i] = data[j];
                data[j] = temp;
            }
        }
    }

    // Calculate the median
    if (size % 2 == 0) {
        return (data[size / 2 - 1] + data[size / 2]) / 2;
    } else {
        return data[size / 2];
    }
}

// Function to perform smart sampling and calculate average voltage
float perform_smart_sampling(adc_cali_handle_t adc1_cali_handle, adc_oneshot_unit_handle_t adc1_handle, adc_channel_t channel, bool do_calibration1_pressure_sensor) {
    int adc_raw;
    int voltage_mv;
    float average_voltage = 0.0;
    uint16_t sensor_samples = (uint16_t) S_DEFAULT_SENSOR_SAMPLING_COUNT;
    uint16_t sensor_smp_int = (uint16_t) S_DEFAULT_SENSOR_SAMPLING_INTERVAL;
    uint16_t sensor_deviate = (uint16_t) S_DEFAULT_SENSOR_SAMPLING_MEDIAN_DEVIATION;

    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_COUNT, &sensor_samples));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_INTERVAL, &sensor_smp_int));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_SENSOR_SAMPLING_MEDIAN_DEVIATION, &sensor_deviate));

    int samples[sensor_samples];
    float filtered_samples[sensor_smp_int];
    int num_filtered_samples = 0;

    // Collect NUM_SAMPLES samples every SAMPLE_INTERVAL_MS
    for (int i = 0; i < sensor_samples; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &adc_raw));
        if (do_calibration1_pressure_sensor) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv));
        }
        samples[i] = voltage_mv;
        vTaskDelay(pdMS_TO_TICKS(sensor_smp_int));
    }

    // Calculate the median of the collected samples
    int median = calculate_median(samples, (int)sensor_samples);

    // Filter samples that differ from the median by more than the threshold percentage
    num_filtered_samples = 0;
    for (int i = 0; i < (int)sensor_samples; i++) {
        float deviation = fabs((float)(samples[i] - median) / median * 100);
        if (deviation <= sensor_deviate) {
            filtered_samples[num_filtered_samples++] = samples[i];
        }
    }

    // Calculate the average of the filtered samples
    for (int i = 0; i < num_filtered_samples; i++) {
        average_voltage += filtered_samples[i];
    }

    if (num_filtered_samples > 0) {
        average_voltage /= num_filtered_samples;
    } else {
        ESP_LOGW("Sampling", "No valid samples after filtering.");
        average_voltage = median;  // Fall back to median if no samples pass the filter
    }

    return average_voltage;
}



/**
 * @brief: Get CJSON object of sensor_data_t
 */
cJSON *sensor_state_to_JSON(sensor_data_t *s_data) {

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    // pressure: 2 decimal places
    cJSON *j_pressure = cJSON_CreateNumber(
        roundf(s_data->pressure * 100.0f) / 100.0f
    );
    if (j_pressure != NULL) {
        cJSON_AddItemToObject(root, "pressure", j_pressure);
    }

    // voltage: 4 decimal places
    cJSON *j_voltage = cJSON_CreateNumber(
        roundf(s_data->voltage * 10000.0f) / 10000.0f
    );
    if (j_voltage != NULL) {
        cJSON_AddItemToObject(root, "voltage", j_voltage);
    }

    // voltage_offset: 3 decimal places
    cJSON *j_voltage_offset = cJSON_CreateNumber(
        roundf(s_data->voltage_offset * 1000.0f) / 1000.0f
    );
    if (j_voltage_offset != NULL) {
        cJSON_AddItemToObject(root, "voltage_offset", j_voltage_offset);
    }

    // sensor_linear_multiplier: raw
    cJSON *j_sensor_linear_multiplier = cJSON_CreateNumber(
        s_data->sensor_linear_multiplier
    );
    if (j_sensor_linear_multiplier != NULL) {
        cJSON_AddItemToObject(root, "sensor_linear_multiplier", j_sensor_linear_multiplier);
    }

    // voltage_raw: raw
    cJSON *j_voltage_raw = cJSON_CreateNumber(
        s_data->voltage_raw
    );
    if (j_voltage_raw != NULL) {
        cJSON_AddItemToObject(root, "voltage_raw", j_voltage_raw);
    }

    return root;
}

/**
 * @brief: Serialize pressure sensor data to JSON
 *
 */
char *serialize_sensor_state(sensor_data_t *s_data) {
    char *json = NULL;

    // Debugging: Print sensor data before serializing
    /*
    ESP_LOGD(TAG, "Data for the serialization (in function): Raw ADC Value: %d, Voltage: %.3f V, Pressure: %.3f Pa", 
             s_data->voltage_raw, s_data->voltage, s_data->pressure);
    */

    cJSON *c_json = sensor_state_to_JSON(s_data);
    json = cJSON_Print(c_json);
    cJSON_Delete(c_json);
    return json;
}


/**
 * @brief: Get CJSON object of sensor_status_t
 */
cJSON *sensor_status_to_JSON(sensor_status_t *s_data) {

    cJSON *root = cJSON_CreateObject();

    cJSON *j_free_heap = cJSON_CreateNumber(s_data->free_heap);
    if (j_free_heap != NULL) {
        cJSON_AddItemToObject(root, "free_heap", j_free_heap);
    }

    cJSON *j_min_free_heap = cJSON_CreateNumber(s_data->min_free_heap);
    if (j_min_free_heap != NULL) {
        cJSON_AddItemToObject(root, "min_free_heap", j_min_free_heap);
    }

    cJSON *j_time_since_boot = cJSON_CreateNumber(s_data->time_since_boot);
    if (j_time_since_boot != NULL) {
        cJSON_AddItemToObject(root, "time_since_boot", j_time_since_boot);
    }

#if _DEVICE_ENABLE_STATUS_MEMGUARD
    cJSON *j_memguard_threshold = cJSON_CreateNumber(s_data->memguard_threshold);
    if (j_memguard_threshold != NULL) {
        cJSON_AddItemToObject(root, "memguard_threshold", j_memguard_threshold);
    }

    cJSON *j_memguard_mode = cJSON_CreateNumber(s_data->memguard_mode);
    if (j_memguard_mode != NULL) {
        cJSON_AddItemToObject(root, "memguard_mode", j_memguard_mode);
    }
#endif

    return root;

}

/**
 * @brief: Serialize sensor status information to JSON string
 */
char *serialize_sensor_status(sensor_status_t *s_data) {

    char *json = NULL;
    cJSON *c_json = sensor_status_to_JSON(s_data);

    json = cJSON_Print(c_json);
    cJSON_Delete(c_json);
    return json;

}

/**
 * @brief: Compile JSON object from sensor state and device status 
 */
cJSON *sensor_all_to_JSON(sensor_status_t *status, sensor_data_t *sensor) {

    cJSON *root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "sensor", sensor_state_to_JSON(sensor));
    cJSON_AddItemToObject(root, "status", sensor_status_to_JSON(status));

    return root;

}

/**
 * @brief: Serialize all device data (sensor, status) to JSON
 */
char *serialize_all_device_data(sensor_status_t *status, sensor_data_t *sensor) {

    char *json = NULL;
    cJSON *c_json = sensor_all_to_JSON(status, sensor);

    json = cJSON_Print(c_json);
    cJSON_Delete(c_json);
    return json;

}