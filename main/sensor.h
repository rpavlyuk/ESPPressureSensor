#ifndef SENSOR_H
#define SENSOR_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define PRESSURE_SENSOR_PIN     ADC_CHANNEL_3           // GPIO3 corresponds to ADC_CHANNEL_3 on the ESP32-C6
#define ADC_WIDTH               ADC_WIDTH_BIT_12        // 12-bit ADC width for higher resolution
#define ADC_ATTEN               ADC_ATTEN_DB_2_5        // Set attenuation

/**
 * Sensor readings information
 */
typedef struct {
    float voltage;
    int voltage_raw;
    float voltage_offset;
    float pressure;
    uint32_t sensor_linear_multiplier;
} sensor_data_t;

extern sensor_data_t sensor_data;

sensor_data_t get_sensor_data();
bool sensor_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
void sensor_adc_calibration_deinit(adc_cali_handle_t handle);

int calculate_median(int* data, int size);
float perform_smart_sampling(adc_cali_handle_t adc1_cali_handle, adc_oneshot_unit_handle_t adc1_handle, adc_channel_t channel, bool do_calibration1_pressure_sensor);

void sensor_run(void *pvParameters);

#endif