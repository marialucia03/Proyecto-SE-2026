#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_now.h"
#include "comunicaciones.h"
#include "display.h"
#include "ppg_bpm.h"
#include "max30100.h"

/// Variables de configuración de ESP-NOW
#define CANAL_ESPNOW 1
static const uint8_t DIRC_MAC_MASTER[] = {
    0xB0,
    0xCB,
    0xD8,
    0xE8,
    0x67,
    0x78
};

/// Variables de configuración de ADC
#define ADC_UNIT ADC_UNIT_1
#define BPM_CHAN ADC_CHANNEL_3
#define ADC_SAMPLE_PERIOD_MS 20000



int valor_raw = 0;
int valor_mv = 0;
int bpm_local = 0; // BPM medido localmente en el slave
int bpm_raw=0;
int bpm_vm = 0;
int pantalla_actual = 0;
static bool pausa_activa_anterior = false;



/// Variables de  I2C
#define I2C_SCL_PIN 1
#define I2C_SDA_PIN 0


///  Variables de estado para logs
typedef struct {
    bool status_wifi_init;
    bool status_espnow_init;
    bool status_adc_init;
    int i2c_init;
} logs_inits_t;
typedef struct {
    bool status_conexion;
    bool status_temp;
    bool status_bpm;
    bool status_display;
} logs_status_t;

logs_inits_t logs_inits = {
    .status_wifi_init = false,
    .status_espnow_init = false,
    .status_adc_init = false,
    .i2c_init = -1
};

logs_status_t logs_status_actual = {
    .status_conexion = false,
    .status_temp = false,
    .status_bpm = false,
    .status_display = false,
};
logs_status_t logs_status_anterior = {
    .status_conexion = false,
    .status_temp = false,
    .status_bpm = false,
    .status_display = false,
};

/* Mutex to protect shared I2C bus; drivers expect this external symbol */
SemaphoreHandle_t g_i2c_mutex = NULL;


/// Funciones de inicialización

// Funciones del sistema de comunicaciones ESP-NOW: Configura WiFi, inicializa ESP-NOW, registra el callback de recepción y configura el pin del LED para indicar estado de conexión.
static void coms_init(void){
    gpio_set_direction(8, GPIO_MODE_OUTPUT);
    configuracion_wifi(CANAL_ESPNOW, &logs_inits.status_wifi_init);
    configuracion_espnow(CANAL_ESPNOW, DIRC_MAC_MASTER,&logs_inits.status_espnow_init);
    esp_now_register_recv_cb(recibir_datos);
}


// Funciones del sistema de ADC: Configura el ADC para leer el sensor PPG y configura un timer para controlar el periodo de muestreo.
static void bpm_init(void){
    configuracion_max30100(&logs_inits.status_adc_init);
}


static void display_init(void){
    configuracion_i2c(0, I2C_SDA_PIN, I2C_SCL_PIN, &logs_inits.i2c_init);
    // Configure three screen buttons and a fourth button for PWM override (GPIO_NUM_4)
    configuracion_control_display(GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_4, GPIO_NUM_7);
    inicializar_display(&logs_status_actual.status_display);
    pantalla_actual = 0;

}


/// Tareas del sistema

// Tarea de comunicación ESP-NOW: Envía bpm local al master
void tarea_envio_datos(void *pvParameters) {
    while (1) {
        bool modo_pausa = display_en_pausa();

        logs_status_actual.status_temp = false;
        logs_status_actual.status_display = display_status_ok();
        logs_status_actual.status_bpm = modo_pausa ? false : ppg_bpm_signal_ok();

        if (modo_pausa) {
            display_forzar_pwm_off();
        }

        datos_slave_t datos = {
            .bpm = modo_pausa ? 0 : bpm_local,
            .status_i2c_init = logs_inits.i2c_init,
            .status_display = logs_status_actual.status_display,
            .status_bpm_signal = logs_status_actual.status_bpm,
            .pwm_override = modo_pausa ? false : display_get_pwm_override(),
        };
        enviar_datos_slave(&datos, DIRC_MAC_MASTER);
        // If the override was pending, clear the pending flag after sending
        if (display_pwm_override_pending()) {
            display_clear_pwm_override();
        }
        // Aumentado de 50ms a 200ms para reducir ruido RF causado por ESP-NOW
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


// Tarea de chequeo de conexion ESP-NOW: Realiza un ping-pong para verificar la conexión con el master y actualiza el estado de conexión.
void tarea_checkeo_conexion(void *pvParameters) {
    while (1) {
        ping_pong(DIRC_MAC_MASTER, &logs_status_actual.status_conexion, 8, false);
       vTaskDelay(pdMS_TO_TICKS(5000)); // Verificar cada 5 segundos
    }
}


// Tarea de control de display: Actualiza la pantalla según el estado actual y los datos recibidos del master. Cambia de pantalla si hay una solicitud pendiente.
void tarea_control_display(void *pvParameters) {
    while (1) {
        if (display_cambio_pantalla_pendiente()) {
            pantalla_actual = display_pantalla_solicitada();
            display_limpiar_cambio_pantalla();
        }

        bool modo_pausa = display_en_pausa();

        if (pantalla_actual == 0 && display_apagado()) {
            display_encender();
        }

        if (modo_pausa && !pausa_activa_anterior) {
            display_forzar_pwm_off();
            bpm_local = 0;
            pausa_activa_anterior = true;
        } else if (!modo_pausa && pausa_activa_anterior) {
            pausa_activa_anterior = false;
        }

        if (pantalla_actual == 0) {
            datos_master_t datos_recibidos;
            obtener_ultimo_datos_master(&datos_recibidos);
            // Mostrar BPM medido localmente, temperatura y estado PWM recibido desde el master
            pantalla_datos_biometricos(bpm_local, datos_recibidos.temperatura, datos_recibidos.pwm_on);
        } else if (pantalla_actual == 1) {
            pantalla_conexion_espnow(logs_status_actual.status_conexion);
        } else if (pantalla_actual == 2) {
            pantalla_pausa();
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Actualizar cada 100 ms
    }
} 


// Tarea de lectura de BPM: Lee el sensor PPG, actualiza el buffer de muestras y calcula el BPM cada vez que el buffer se llena. El BPM calculado se almacena en bpm_local para ser enviado al master.
void tarea_bpm(void *arg)
{
    uint16_t ir_value = 0;
    uint16_t red_value = 0;

    while (1)
    {
        if (display_en_pausa()) {
            bpm_local = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        esp_err_t ret =
        max30100_read_fifo(&ir_value);

        if (ret == ESP_OK)
        {
            calculo_bpm(
                ir_value,
                xTaskGetTickCount() *
                portTICK_PERIOD_MS);
            /* Actualizar variable local con el BPM calculado */
            bpm_local = ppg_bpm_get();
            printf(
                "IR: %u RED: %u BPM: %u OK: %d\n",
                ir_value,
                red_value,
                ppg_bpm_get(),
                ppg_bpm_signal_ok());
        }
        else
        {
            printf(
                "Error lectura MAX30100\n");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


/// Función principal de la aplicación
void app_main() {

    // Inicialización de NVS (necesaria para WiFi y ESP-NOW)
    esp_err_t ret= nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        }

    /* Create I2C mutex before initializing drivers that use I2C */
    g_i2c_mutex = xSemaphoreCreateMutex();
    if (g_i2c_mutex == NULL) {
        return; /* cannot continue without I2C protection */
    }

    // Inicialización de componentes
    coms_init();
    display_init();
    bpm_init();
    // Creación de tareas 
    xTaskCreate(tarea_envio_datos, "tarea_envio_datos", 4096, NULL, 5, NULL);
    xTaskCreate(tarea_control_display, "tarea_control_display",2048, NULL, 4, NULL);
    xTaskCreate(tarea_checkeo_conexion, "tarea_checkeo_conexion", 2048, NULL, 3, NULL);
    xTaskCreate(tarea_bpm, "tarea_bpm", 4096, NULL, 5, NULL);

    while (1) { 
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
