#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include <math.h>

#include "common.h"
#include "sensor.h"
#include "settings.h"
#include "mqtt.h"
#include "zigbee.h"
#include "non_volatile_storage.h"

sensor_data_t sensor_data;

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
    ESP_LOGI(TAG, "Waiting for device to become ready");
    int attempt = 0;
    int max_attempts = 255;
    while(device_ready < 1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGD(TAG, "Attempt #%i", attempt++);
        if (attempt > max_attempts)
        {
            ESP_LOGI(TAG, "Max attempts to make device ready reached. Trying to continue and hoping for the best...");
            break;
        }      
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

        if (_DEVICE_ENABLE_MQTT) {
            ESP_LOGI(TAG, "Sensor Run - Before MQTT::Publish - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));

            // Publish the sensor data via MQTT
            mqtt_publish_sensor_data(&sensor_data);

            ESP_LOGI(TAG, "Sensor Run - After MQTT::Publish - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
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
