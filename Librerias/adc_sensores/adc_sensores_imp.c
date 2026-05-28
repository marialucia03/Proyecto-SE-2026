#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/timer.h"
#include <stdbool.h>
#include "adc_sensores.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


static SemaphoreHandle_t adc_mutex = NULL;
static SemaphoreHandle_t semaforo_muestreo = NULL;
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle = NULL;

void configuracion_adc(adc_unit_t unit_id,adc_channel_t PIN_ADC, bool *status_adc_init) {
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = unit_id,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &adc_handle));   

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(
            adc_handle,
            PIN_ADC,
            &channel_config
        ));
    

    /*
        controlador = true  -> ESP32 clasico
        controlador = false -> ESP32-C3/S3
    */

    #if CONFIG_IDF_TARGET_ESP32

    /* ESP32 clasico -> LINE FITTING */

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit_id,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(
        adc_cali_create_scheme_line_fitting(
            &cali_config,
            &adc_cali_handle
        )
    );

    #else

    /* ESP32-C3 / S3 / C6 -> CURVE FITTING */

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit_id,
        .chan = PIN_ADC,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(
        adc_cali_create_scheme_curve_fitting(
            &cali_config,
            &adc_cali_handle
        )
    );

    #endif

    // Mutex para proteger acceso a ADC
    if (adc_mutex == NULL) {
        adc_mutex = xSemaphoreCreateMutex();
    }

    // Semaforo para controlar el muestreo
    if (semaforo_muestreo == NULL) {
        semaforo_muestreo = xSemaphoreCreateBinary();
    }
    if (status_adc_init != NULL) {
        *status_adc_init = 1;
    }
}

bool  IRAM_ATTR callback_timer_adc(void *arg){
    (void)arg; // Evita advertencia de variable no usada
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Despierta a la tarea de lectura de forma eficiente
    xSemaphoreGiveFromISR(semaforo_muestreo, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken==pdTRUE);
}
void configuracion_timer_adc(timer_group_t timer_group, timer_idx_t timer_idx, int tiempo_muestreo_us){
    timer_config_t config = {
        .divider = 80, // 1us per tick
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
    };
    timer_init(timer_group, timer_idx, &config);
    timer_set_alarm_value(timer_group, timer_idx, tiempo_muestreo_us); // 1 second
    timer_enable_intr(timer_group, timer_idx);
    timer_isr_callback_add(timer_group, timer_idx, callback_timer_adc, NULL, 0);
    timer_start(timer_group, timer_idx);
}

int lectura_adc(adc_channel_t channel, int *raw_value, int *voltage_mv){
    if (xSemaphoreTake(semaforo_muestreo, portMAX_DELAY) == pdTRUE) {
        
        // Protege el hardware por si otra tarea quiere leer el ADC
        if (xSemaphoreTake(adc_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            adc_oneshot_read(adc_handle, channel, raw_value);
            
            if (adc_cali_handle != NULL) {
                adc_cali_raw_to_voltage(adc_cali_handle, *raw_value, voltage_mv);
            }
            xSemaphoreGive(adc_mutex);
            return 0; // Éxito
        }
    }
    return -1; // Error o Timeout
}

