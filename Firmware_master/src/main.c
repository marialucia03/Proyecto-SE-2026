// Librerias usadas 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "comunicaciones.h"
#include "adc_sensores.h"
#include "bluetooth_spp.h"
#include "pwm.h"


///Variables configuracion espnow
#define CANAL_ESPNOW 1
#define LED_PIN GPIO_NUM_2
static const uint8_t DIRC_MAC_SLAVE[] = {
    0xD8,
    0x3B,
    0xDA,
    0x27,
    0xA5,
    0xB0
};
static int bpm_slave_recibido = 0;

/// Variables de configuración de ADC
#define ADC_SAMPLE_PERIOD_MS 20000
#define TEMP_SEN_PIN ADC_CHANNEL_3
#define ADC_UNIT ADC_UNIT_1
#define TEMP_MOV_AVG_SAMPLES 16

// variables de lectura 
static int valor_raw = 0;
static int valor_mv = 0;
static int temperatura_medida = 0;

// Variables para promedio móvil de temperatura
static int temp_mov_buffer[TEMP_MOV_AVG_SAMPLES] = {0};
static int temp_mov_sum = 0;
static int temp_mov_index = 0;
static int temp_mov_count = 0;


/// Variables de control de pwm 
#define PWM_FREQUENCY_HZ 5000
#define PWM_TIMER LEDC_TIMER_0
#define PWM_CHANNEL LEDC_CHANNEL_0
#define PWM_RESOLUTION LEDC_TIMER_10_BIT
#define PWM_PIN GPIO_NUM_23

bool alarma_temperatura = false;
bool last_alarma_temperatura = false;
bool alarm_bpm = false;
bool last_alarm_bpm = false;

/// Variables de configuración de Bluetooth
#define BLE_LOG_PERIOD_MS 5000

logs_inits_t logs_inits = {
    .status_wifi_init = false,
    .status_espnow_init = false,
    .status_adc_init = false,
    .status_i2c_init = false,
    .status_pwm_init = false,
};

logs_status_t logs_status_actuales= {
    .status_pwm = false,
    .status_conexion = false,
    .status_lectura = false,
};
logs_status_t logs_status_anteriores = {
    .status_pwm = false,
    .status_conexion = false,
    .status_lectura = false,
};

volatile bool boot_completed = false;
static TaskHandle_t ble_log_task_handle = NULL;

static inline void ble_notify_status_change(void)
{
    if (ble_log_task_handle != NULL) {
        xTaskNotifyGive(ble_log_task_handle);
    }
}

static void ble_tx_ready_changed(bool tx_ready, void *ctx)
{
    (void)tx_ready;
    (void)ctx;
    ble_notify_status_change();
}



/// Funciones de configuracion inicial

// Configuracion de comunicaciones (WiFi, ESP-NOW, Bluetooth)
static void comunicacions_init(void)
{
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    configuracion_wifi(CANAL_ESPNOW, &logs_inits.status_wifi_init);
    bluetooth_init();
    ble_register_tx_ready_callback(ble_tx_ready_changed, NULL);
    configuracion_espnow(CANAL_ESPNOW, DIRC_MAC_SLAVE, &logs_inits.status_espnow_init);
    esp_now_register_recv_cb(recibir_datos);
}

// Configuracion de sensores (ADC)
static void adc_sensores_init(void)
{
    configuracion_adc(ADC_UNIT, TEMP_SEN_PIN, &logs_inits.status_adc_init);
    configuracion_timer_adc(TIMER_GROUP_0, TIMER_0, ADC_SAMPLE_PERIOD_MS);
}

// Configuracion de actuadores (PWM)
static void pwm_init(void)
{   
    configuracion_pwm(PWM_PIN, PWM_CHANNEL, PWM_TIMER, PWM_FREQUENCY_HZ, PWM_RESOLUTION, &logs_inits.status_pwm_init);
}

/// Tareas del sistema 

// Tarea de lectura de sensores: Lee el sensor de temperatura, actualiza el promedio móvil y verifica si se activa la alarma de temperatura.
static void tarea_lectura_sensores(void *pvParameters)
{
    while (1) {
        bool prev_status_lectura = logs_status_actuales.status_lectura;

        if (lectura_adc(TEMP_SEN_PIN, &valor_raw, &valor_mv) == 0) {
            int current_temp = valor_mv / 10;

            /* Actualizar buffer de promedio móvil */
            if (temp_mov_count < TEMP_MOV_AVG_SAMPLES) {
                temp_mov_sum += current_temp;
                temp_mov_buffer[temp_mov_index] = current_temp;
                temp_mov_index = (temp_mov_index + 1) % TEMP_MOV_AVG_SAMPLES;
                temp_mov_count++;
            } else {
                /* Reemplaza el valor más antiguo */
                temp_mov_sum -= temp_mov_buffer[temp_mov_index];
                temp_mov_buffer[temp_mov_index] = current_temp;
                temp_mov_sum += current_temp;
                temp_mov_index = (temp_mov_index + 1) % TEMP_MOV_AVG_SAMPLES;
            }

            /* Calcular promedio (soporta inicio con menos muestras) */
            temperatura_medida = temp_mov_sum / (temp_mov_count > 0 ? temp_mov_count : 1);

            if (temperatura_medida < -40) {
                temperatura_medida = -40;
            }

            if (temperatura_medida > 125) {
                temperatura_medida = 125;
            }

            logs_status_actuales.status_lectura = true;
        } else {
            logs_status_actuales.status_lectura = false;
        }

        if (logs_status_actuales.status_lectura != prev_status_lectura) {
            ble_notify_status_change();
        }

        if (temperatura_medida >=37){
            last_alarma_temperatura = alarma_temperatura;
            alarma_temperatura = true;
        }
        else {
            last_alarma_temperatura = alarma_temperatura;
            alarma_temperatura = false;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Tarea de envío de datos: Envía periódicamente los datos de temperatura y recibe del slave a través de ESP-NOW.
static void tarea_envio_datos(void *pvParameters)
{
    while (1) {
        datos_t datos = {
            .voltaje_potenciometro = valor_mv,
            .temperatura = temperatura_medida,
            .bpm = bpm_slave_recibido,
        };
        enviar_datos(&datos, DIRC_MAC_SLAVE);
        // Reducido a 200ms para minimizar ruido RF causado por ESP-NOW
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Tarea de checkeo de conexion ESP-NOW: Realiza un ping-pong para verificar la conexión con el slave y actualiza el estado de conexión.    
static void tarea_checkeo_conexion(void *pvParameters)
{
    while (1) {
        bool prev_status_conexion = logs_status_actuales.status_conexion;
        ping_pong(DIRC_MAC_SLAVE, &logs_status_actuales.status_conexion, LED_PIN, true);
        if (logs_status_actuales.status_conexion != prev_status_conexion) {
            ble_notify_status_change();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Tarea de control de PWM: control basado en BPM y temperatura.
static void tarea_control_pwm(void *pvParameters)
{
    while (1) {
        bool prev_status_pwm = logs_status_actuales.status_pwm;

        if (alarma_temperatura == true && last_alarma_temperatura == false && alarm_bpm == false && last_alarm_bpm == false) {
            encender_pwm_suave(2000, 20, &logs_status_actuales.status_pwm);
        } else if (alarma_temperatura == false && last_alarma_temperatura == true && alarm_bpm == false && last_alarm_bpm == false) {
            apagar_pwm_suave(2000, 20, &logs_status_actuales.status_pwm);
        }

        if (logs_status_actuales.status_pwm != prev_status_pwm) {
            ble_notify_status_change();
        }

        /* Evita monopolizar CPU y permite ejecutar tareas BLE/logs. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Tarea de envío de logs por Bluetooth: Envía periódicamente los logs de inicialización y estado.
static void tarea_logs_bluetooth(void *pvParameters)
{
    bool tx_ready_prev = false;

    while (1) {
        /* Bloquea hasta cambio de estado o timeout de respaldo. */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_LOG_PERIOD_MS));

        bool tx_ready_now = ble_is_tx_ready();

        if (!tx_ready_now) {
            tx_ready_prev = false;
            continue;
        }

        if (!tx_ready_prev) {
            if (boot_completed) {
                ble_send_log_inits(&logs_inits);
                boot_completed = false;
            }
            ble_send_log_status(&logs_status_actuales);
            logs_status_anteriores = logs_status_actuales;
            tx_ready_prev = true;
            continue;
        }

        if (
            logs_status_actuales.status_pwm != logs_status_anteriores.status_pwm ||
            logs_status_actuales.status_conexion != logs_status_anteriores.status_conexion ||
            logs_status_actuales.status_lectura != logs_status_anteriores.status_lectura
        ) {
            ble_send_log_status(&logs_status_actuales);
            logs_status_anteriores = logs_status_actuales;
        }
    }
}

/// Función principal de la aplicación
void app_main(void)
{   
    // Inicialización de NVS (necesaria para WiFi y ESP-NOW)
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Inicialización de componentes
    comunicacions_init();
    adc_sensores_init();
    pwm_init();
    boot_completed = true; 
    // Creación de tareas
    xTaskCreate(tarea_lectura_sensores, "tarea_lectura_sensores", 4096, NULL, 5, NULL);
    xTaskCreate(tarea_envio_datos, "tarea_envio_datos", 4096, NULL, 5, NULL);
    xTaskCreate(tarea_control_pwm, "tarea_control_pwm", 1024, NULL, 4, NULL);
    xTaskCreate(tarea_checkeo_conexion, "tarea_checkeo_conexion", 2048, NULL, 3, NULL);
    xTaskCreate(tarea_logs_bluetooth, "tarea_logs_bluetooth", 4096, NULL, 2, &ble_log_task_handle);

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}