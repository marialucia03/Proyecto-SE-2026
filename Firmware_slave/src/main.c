#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_now.h"
#include "comunicaciones.h"
#include "adc_sensores.h"
#include "display.h"
#include "ppg_bpm.h"

/// Variables de configuración de ESP-NOW
#define CANAL_ESPNOW 1
static const uint8_t DIRC_MAC_MASTER[] = {
    0x20,
    0xE7,
    0xC8,
    0xAC,
    0x9E,
    0x24
};

/// Variables de configuración de ADC
#define ADC_UNIT ADC_UNIT_1
#define BPM_CHAN ADC_CHANNEL_3
#define ADC_SAMPLE_PERIOD_MS 20000
#define BUFFER_SIZE 10
#define PPG_BPM_BUFFER_SIZE 100

int valor_raw = 0;
int valor_mv = 0;
int bpm_local = 0; // BPM medido localmente en el slave
int bpm_raw=0;
int bpm_vm = 0;
int pantalla_actual = 0;
static int bpm_muestras[PPG_BPM_BUFFER_SIZE] = {0};
static int bpm_indice_muestra = 0;
static bool bpm_buffer_completo = false;


/// Variables de  I2C
#define I2C_SCL_PIN 1
#define I2C_SDA_PIN 0


///  Variables de estado para logs
typedef struct {
    bool status_wifi_init;
    bool status_espnow_init;
    bool status_adc_init;
    bool i2c_init;
} logs_inits_t;
typedef struct {
    bool status_conexion;
    bool status_lectura;
} logs_status_t;

logs_inits_t logs_inits = {
    .status_wifi_init = false,
    .status_espnow_init = false,
    .status_adc_init = false
};

logs_status_t logs_status_actual = {
    .status_conexion = false,
};
logs_status_t logs_status_anterior = {
    .status_conexion = false,
};


/// Funciones de inicialización

// Funciones del sistema de comunicaciones ESP-NOW: Configura WiFi, inicializa ESP-NOW, registra el callback de recepción y configura el pin del LED para indicar estado de conexión.
static void coms_init(void){
    gpio_set_direction(8, GPIO_MODE_OUTPUT);
    configuracion_wifi(CANAL_ESPNOW, &logs_inits.status_wifi_init);
    configuracion_espnow(CANAL_ESPNOW, DIRC_MAC_MASTER,&logs_inits.status_espnow_init);
    esp_now_register_recv_cb(recibir_datos);
}


// Funciones del sistema de ADC: Configura el ADC para leer el sensor PPG y configura un timer para controlar el periodo de muestreo.
static void adc_init(void){
    configuracion_adc(ADC_UNIT, BPM_CHAN, &logs_inits.status_adc_init);
    configuracion_timer_adc(TIMER_GROUP_0, TIMER_0, ADC_SAMPLE_PERIOD_MS );
}


static void display_init(void){
    configuracion_i2c(0, I2C_SDA_PIN, I2C_SCL_PIN);
    configuracion_control_display(GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7);
    inicializar_display();
    pantalla_actual = 0;
}


/// Tareas del sistema

// Tarea de comunicación ESP-NOW: Envía bpm local al master
void tarea_envio_datos(void *pvParameters) {
    while (1) {
        datos_t datos = {
            .bpm = bpm_local
        };
        enviar_datos(&datos, DIRC_MAC_MASTER);
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
        if (pantalla_actual == 0) {
                datos_t datos_recibidos;
                obtener_ultimo_datos(&datos_recibidos);
                // Mostrar BPM medido localmente y temperatura recibida desde el master
                pantalla_datos_biometricos(bpm_local, datos_recibidos.temperatura);
        } else if (pantalla_actual == 1) {
            pantalla_conexion_espnow(logs_status_actual.status_conexion);
        } else if (pantalla_actual == 2) {
            pantalla_pausa();
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Actualizar cada 100 ms
    }
} 


// Tarea de lectura de BPM: Lee el sensor PPG, actualiza el buffer de muestras y calcula el BPM cada vez que el buffer se llena. El BPM calculado se almacena en bpm_local para ser enviado al master.
void tarea_get_bpm (void *pvParameters) {
    while (1) {
        if (lectura_adc(BPM_CHAN, &bpm_raw, &bpm_vm) == 0) {
            bpm_muestras[bpm_indice_muestra] = bpm_vm;
            bpm_indice_muestra++;
            if (bpm_indice_muestra >= PPG_BPM_BUFFER_SIZE) {
                bpm_indice_muestra = 0;
                bpm_buffer_completo = true;
            }
            if (bpm_buffer_completo) {
                calculo_bpm(bpm_vm,xTaskGetTickCount()*portTICK_PERIOD_MS);
                bpm_local = ppg_bpm_get();  
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
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

    // Inicialización de componentes
    coms_init();
    adc_init();
    display_init();

    // Creación de tareas 
    xTaskCreate(tarea_envio_datos, "tarea_envio_datos", 4096, NULL, 5, NULL);
    xTaskCreate(tarea_control_display, "tarea_control_display",2048, NULL, 4, NULL);
    xTaskCreate(tarea_checkeo_conexion, "tarea_checkeo_conexion", 2048, NULL, 3, NULL);
    xTaskCreate(tarea_get_bpm, "tarea_get_bpm", 4096, NULL, 5, NULL);

    while (1) { 
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}