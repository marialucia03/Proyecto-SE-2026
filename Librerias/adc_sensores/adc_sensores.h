#ifndef ADC_SENSORES_H
#define ADC_SENSORES_H


#include "stdio.h"
#include "stdbool.h"
#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/timer.h"


void configuracion_adc(adc_unit_t canal, adc_channel_t PIN_ADC, bool *status_adc_init);
void configuracion_timer_adc(timer_group_t timer_group, timer_idx_t timer_idx, int tiempo_muestreo_us);
bool IRAM_ATTR callback_timer_adc(void *arg);
int lectura_adc(adc_channel_t channel, int *raw_value, int *voltage_mv);


#endif