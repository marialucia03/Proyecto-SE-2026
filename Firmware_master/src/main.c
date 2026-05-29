// Librerias usadas 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
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
static int status_i2c_slave = -1;
static bool status_display_slave = false;
static volatile bool pwm_override_received = false;
static volatile bool pwm_override_prev = false;
static volatile bool pwm_override_changed = false;

/// Variables de configuración de ADC
#define ADC_SAMPLE_PERIOD_MS 20000
#define TEMP_SEN_PIN ADC_CHANNEL_6
#define ADC_UNIT ADC_UNIT_1
#define TEMP_MOV_AVG_SAMPLES 16
#define TEMP_OFFSET_DETECTION_SAMPLES 5
#define TEMP_OFFSET_MIN_DELTA_C 2


// Variables de filtro EMA para la temperatura
#define TEMP_EMA_ALPHA 0.08f

// variables de lectura 
static int valor_raw = 0;
static int valor_mv = 0;
static int temperatura_medida = 0;

// Variables para promedio móvil de temperatura
static int temp_mov_buffer[TEMP_MOV_AVG_SAMPLES] = {0};
static int temp_mov_sum = 0;
static int temp_mov_index = 0;
static int temp_mov_count = 0;
static float temperatura_ema = 0.0f;
static bool temperatura_ema_init = false;
static bool pwm_estado_previo_para_offset = false;
static bool temp_offset_calibrando = false;
static int temp_offset_referencia = 0;
static int temp_offset_acumulado = 0;
static int temp_offset_muestras = 0;
static int temp_offset_compensacion = 0;


/// Variables de control de pwm 
#define PWM_FREQUENCY_HZ 500
#define PWM_TIMER LEDC_TIMER_0
#define PWM_CHANNEL LEDC_CHANNEL_0
#define PWM_RESOLUTION LEDC_TIMER_10_BIT
#define PWM_PIN GPIO_NUM_23

#define ALARMA_BPM_THRESHOLD 120
#define TEMP_ACTIVACION 38.0f
bool alarma_temperatura = false;
bool alarma_bpm = false;
static TickType_t tiempo_ambas_alarmas_inicio = 0;
static TickType_t tiempo_ambas_alarmas_falso_inicio = 0;
static bool bomba_bloqueada_apagada = false;

/// Variables de configuración de Bluetooth
#define BLE_LOG_PERIOD_MS 5000

logs_inits_t logs_inits = {
    .status_wifi_init = false,
    .status_espnow_init = false,
    .status_adc_init = false,
    .status_i2c_init = false,
    .status_pwm_init = -1,
};

logs_status_t logs_status_actuales= {
    .status_pwm = -1,
    .status_conexion = false,
    .status_temp = false,
    .status_bpm = false,
    .status_display = false,
};

logs_status_t logs_status_anteriores = {
    .status_pwm = -1,
    .status_conexion = false,
    .status_temp = false,
    .status_bpm = false,
    .status_display = false,
};

volatile bool boot_completed = false;
TaskHandle_t ble_log_task_handle = NULL;


/// Funciones de configuracion inicial
static void request_logs_cb(void) {
        on_logs_request(&logs_inits, &logs_status_actuales);
}

// Funcion para reiniciar el proceso de compensación de offset de temperatura al detectar un cambio en el estado de la bomba (activación/desactivación)
static void reiniciar_compensacion_offset(void)
{
    temp_offset_calibrando = false;
    temp_offset_referencia = 0;
    temp_offset_acumulado = 0;
    temp_offset_muestras = 0;
    temp_offset_compensacion = 0;
    temperatura_ema_init = false;
}


// Configuracion de comunicaciones (WiFi, ESP-NOW, Bluetooth)
static void comunicacions_init(void)
{
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    configuracion_wifi(CANAL_ESPNOW, &logs_inits.status_wifi_init);
    bluetooth_init();
    ble_register_tx_ready_callback(ble_tx_ready_changed, NULL);
    configuracion_espnow(CANAL_ESPNOW, DIRC_MAC_SLAVE, &logs_inits.status_espnow_init);
    ble_register_logs_request_callback(request_logs_cb);
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
    if (logs_inits.status_pwm_init == 1) {
        logs_status_actuales.status_pwm = 0;
        logs_status_anteriores.status_pwm = 0;
    }
}

/// Tareas del sistema 


// Tarea de lectura de sensores: Lee el sensor de temperatura, actualiza el promedio móvil y verifica si se activa la alarma de temperatura.
static void tarea_lectura_sensores(void *pvParameters)
{
    while (1) {
        bool prev_status_temp = logs_status_actuales.status_temp;
        bool bomba_activa = (logs_status_actuales.status_pwm > 0);

        if (lectura_adc(TEMP_SEN_PIN, &valor_raw, &valor_mv) == 0) {
            float current_temp = (float)valor_mv / 10.0f;

            if (!temperatura_ema_init) {
                temperatura_ema = current_temp;
                temperatura_ema_init = true;
            }

            temperatura_ema = (TEMP_EMA_ALPHA * current_temp) + ((1.0f - TEMP_EMA_ALPHA) * temperatura_ema);

            int temp_filtrada = (int)(temperatura_ema + 0.5f);

            if (temp_mov_count < TEMP_MOV_AVG_SAMPLES) {
                temp_mov_sum += temp_filtrada;
                temp_mov_buffer[temp_mov_index] = temp_filtrada;
                temp_mov_index = (temp_mov_index + 1) % TEMP_MOV_AVG_SAMPLES;
                temp_mov_count++;
            } else {
                temp_mov_sum -= temp_mov_buffer[temp_mov_index];
                temp_mov_buffer[temp_mov_index] = temp_filtrada;
                temp_mov_sum += temp_filtrada;
                temp_mov_index = (temp_mov_index + 1) % TEMP_MOV_AVG_SAMPLES;
            }

            int temperatura_bruta = temp_mov_sum / (temp_mov_count > 0 ? temp_mov_count : 1);

            if (bomba_activa != pwm_estado_previo_para_offset) {
                pwm_estado_previo_para_offset = bomba_activa;
                temperatura_ema_init = false;
                if (bomba_activa) {
                    temp_offset_calibrando = true;
                    temp_offset_referencia = temperatura_bruta;
                    temp_offset_acumulado = 0;
                    temp_offset_muestras = 0;
                    temp_offset_compensacion = 0;
                } else {
                    reiniciar_compensacion_offset();
                }
            }

            if (temp_offset_calibrando && bomba_activa) {
                int delta = temperatura_bruta - temp_offset_referencia;

                if (delta > TEMP_OFFSET_MIN_DELTA_C || delta < -TEMP_OFFSET_MIN_DELTA_C) {
                    temp_offset_acumulado += delta;
                    temp_offset_muestras++;

                    if (temp_offset_muestras > 0) {
                        int estimacion_offset = temp_offset_acumulado / temp_offset_muestras;

                        if (estimacion_offset > -TEMP_OFFSET_MIN_DELTA_C && estimacion_offset < TEMP_OFFSET_MIN_DELTA_C) {
                            estimacion_offset = 0;
                        }

                        temp_offset_compensacion = estimacion_offset;
                    }

                    if (temp_offset_muestras >= TEMP_OFFSET_DETECTION_SAMPLES) {
                        temp_offset_calibrando = false;
                    }
                }
            }

            temperatura_medida = temperatura_bruta - temp_offset_compensacion;

            if (temperatura_medida < -40) {
                temperatura_medida = -40;
            }

            if (temperatura_medida > 125) {
                temperatura_medida = 125;
            }

            logs_status_actuales.status_temp = true;
        } else {
            logs_status_actuales.status_temp = false;
        }

        if (logs_status_actuales.status_temp != prev_status_temp) {
            ble_notify_status_change();
        }

        alarma_temperatura = (temperatura_medida >= TEMP_ACTIVACION);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


// Tarea de envío de datos: Envía periódicamente los datos de temperatura y recibe del slave a través de ESP-NOW.
static void tarea_envio_datos(void *pvParameters)
{
    while (1) {

        datos_slave_t datos_recibidos = {0};
        obtener_ultimo_datos(&datos_recibidos);
        bpm_slave_recibido = datos_recibidos.bpm;
        // detect change on pwm_override from slave
        pwm_override_prev = pwm_override_received;
        pwm_override_received = datos_recibidos.pwm_override;
        pwm_override_changed = (pwm_override_received != pwm_override_prev);
        status_i2c_slave = datos_recibidos.status_i2c_init;
        logs_inits.status_i2c_init = (status_i2c_slave == 1);
        alarma_bpm = (bpm_slave_recibido > ALARMA_BPM_THRESHOLD);

         // Preparar datos para enviar al master

        logs_status_actuales.status_bpm = datos_recibidos.status_bpm_signal;
        status_display_slave = datos_recibidos.status_display;

        datos_master_t datos = {
            .temperatura = temperatura_medida,
            .bpm = bpm_slave_recibido,
            .status_display = status_display_slave,
            .status_temp = logs_status_actuales.status_temp,
            .status_bpm = logs_status_actuales.status_bpm,
            .pwm_on = (logs_status_actuales.status_pwm > 0),
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
        // If slave requested manual PWM override, force pump on
        if (pwm_override_received) {
            /* Slave requests manual override ON: force pump on */
            if (logs_status_actuales.status_pwm == 0) {
                encender_pwm_suave(500, 20, &(logs_status_actuales.status_pwm));
            }
            /* keep forcing while override true */
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* If override was just turned OFF by slave, force a smooth stop immediately */
        if (pwm_override_changed && !pwm_override_received) {
            if (logs_status_actuales.status_pwm > 0) {
                apagar_pwm_suave(500, 20, &(logs_status_actuales.status_pwm));
            }
            pwm_override_changed = false;
            /* allow normal logic to continue after forcing off */
        }
        int prev_status_pwm = logs_status_actuales.status_pwm;
        if (alarma_temperatura && alarma_bpm) {
            /* Reset off-delay and clear any off-lock when both alarms present */
            tiempo_ambas_alarmas_falso_inicio = 0;
            if (tiempo_ambas_alarmas_inicio == 0) {
                tiempo_ambas_alarmas_inicio = xTaskGetTickCount();
            }

            if ((xTaskGetTickCount() - tiempo_ambas_alarmas_inicio) >= pdMS_TO_TICKS(10000) &&
                logs_status_actuales.status_pwm == 0) {
                /* Re-arm and turn on pump */
                bomba_bloqueada_apagada = false;
                encender_pwm_suave(2000, 20, &(logs_status_actuales.status_pwm));
            }
        } else {
            /* Reinicia contador de tiempo conjunto */
            tiempo_ambas_alarmas_inicio = 0;

            /* Si la bomba está encendida y no está ya bloqueada, iniciar conteo de apagado de 10s */
            if (logs_status_actuales.status_pwm > 0 && !bomba_bloqueada_apagada) {
                if (tiempo_ambas_alarmas_falso_inicio == 0) {
                    tiempo_ambas_alarmas_falso_inicio = xTaskGetTickCount();
                }

                if ((xTaskGetTickCount() - tiempo_ambas_alarmas_falso_inicio) >= pdMS_TO_TICKS(10000)) {
                    apagar_pwm_suave(2000, 20, &(logs_status_actuales.status_pwm));
                    /* Bloquea para evitar repetidos comandos de apagado mientras la bandera quede en false */
                    bomba_bloqueada_apagada = true;
                    tiempo_ambas_alarmas_falso_inicio = 0;
                }
            } else {
                /* Si la bomba ya está apagada, limpiar contador de falso */
                tiempo_ambas_alarmas_falso_inicio = 0;
            }
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
            logs_status_actuales.status_temp != logs_status_anteriores.status_temp ||
            logs_status_actuales.status_bpm != logs_status_anteriores.status_bpm ||
            logs_status_actuales.status_display != logs_status_anteriores.status_display
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

    // Creación de tareas
    xTaskCreate(tarea_lectura_sensores, "tarea_lectura_sensores", 4096, NULL, 5, NULL);
    xTaskCreate(tarea_envio_datos, "tarea_envio_datos", 4096, NULL, 5, NULL);
    xTaskCreate(tarea_control_pwm, "tarea_control_pwm", 1024, NULL, 4, NULL);
    xTaskCreate(tarea_checkeo_conexion, "tarea_checkeo_conexion", 2048, NULL, 3, NULL);
    xTaskCreate(tarea_logs_bluetooth, "tarea_logs_bluetooth", 4096, NULL, 2, &ble_log_task_handle);

    boot_completed = true; 
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}
