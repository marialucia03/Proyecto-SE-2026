#include <string.h>
#include "comunicaciones.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"


///  Variables de estado de conexion
typedef struct {
    bool ping;
    bool pong;
} estado_conexion_t;

estado_conexion_t status_check_MS = {
    .ping = true,
    .pong = false
};
estado_conexion_t status_check_SM = {
    .ping = false,
    .pong = true
};

static volatile TickType_t tiempo_ultimo_ping_rx = 0;
static volatile TickType_t tiempo_ultimo_pong_rx = 0;
static volatile TickType_t tiempo_ultimo_ping = 0;


/// Variable para almacenar los últimos datos recibidos 
static volatile datos_slave_t ultimo_datos = {0};
// Además guardamos los últimos datos enviados por el maestro (esclavo necesita leerlos)
static volatile datos_master_t ultimo_datos_master = {0};
// Callback para petición de logs (registrado por la aplicación si la tiene)


/// Funciones de la libreria

// Función para enviar el estado de conexión (ping o pong) al peer
static void enviar_estado_conexion(const estado_conexion_t *estado, const uint8_t *peer_mac) {
    esp_now_send(peer_mac, (const uint8_t *)estado, sizeof(*estado));
}


// Función para configurar WiFi en modo estación y establecer el canal
void configuracion_wifi(int canal,bool *status_wifi_init) {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_wifi_set_storage(WIFI_STORAGE_RAM)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    esp_err_t err_start = esp_wifi_start();
    ESP_ERROR_CHECK(err_start);

    esp_err_t err_chan = esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
    ESP_ERROR_CHECK(err_chan);

    if (status_wifi_init != NULL) {
        *status_wifi_init = true;
    }
}


// Función para configurar ESP-NOW y agregar un peer
void configuracion_espnow(int canal , const uint8_t *peer_mac, bool *status_espnow_init) {
    ESP_ERROR_CHECK(esp_now_init());

    // Reducir potencia TX para minimizar ruido RF
    // Valores: 80 (20dBm max), 60 (18dBm), 40 (16dBm)
    esp_wifi_set_max_tx_power(100);

    esp_now_peer_info_t peer_info = {0};

    memcpy(peer_info.peer_addr,
           peer_mac,
           6);
    peer_info.channel = canal;
    peer_info.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
    if (status_espnow_init != NULL) {
        *status_espnow_init = true;
    }
}


// Función para enviar datos al peer utilizando ESP-NOW
void enviar_datos(const datos_master_t *datos, const uint8_t *peer_mac) {
    esp_now_send(peer_mac, (const uint8_t *)datos, sizeof(datos_master_t));
}

// Enviar datos desde el esclavo al maestro
void enviar_datos_slave(const datos_slave_t *datos, const uint8_t *peer_mac) {
    esp_now_send(peer_mac, (const uint8_t *)datos, sizeof(datos_slave_t));
}


// Función de callback para recibir datos por ESP-NOW
void recibir_datos(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(estado_conexion_t)) {
        estado_conexion_t estado;
        memcpy(&estado, data, sizeof(estado));
        TickType_t tick_actual = xTaskGetTickCount();
        if (estado.ping && recv_info != NULL) {
            tiempo_ultimo_ping_rx = tick_actual;
            estado_conexion_pong(recv_info->src_addr);
        }
        if (estado.pong) {
            tiempo_ultimo_pong_rx = tick_actual;
        }
        return;
    }
    // No procesar peticiones "logs" por ESP-NOW (se manejan por BLE)
    // Si recibimos datos del maestro (datos_master_t)
    if (len == sizeof(datos_master_t)) {
        datos_master_t datos_m;
        memcpy(&datos_m, data, sizeof(datos_m));
        memcpy((void *)&ultimo_datos_master, &datos_m, sizeof(datos_master_t));
        return;
    }
    // Si recibimos datos del esclavo (datos_slave_t)
    if (len == sizeof(datos_slave_t)) {
        datos_slave_t datos_s;
        memcpy(&datos_s, data, sizeof(datos_s));
        // Guardar copia de los datos recibidos para que la aplicación los consulte
        memcpy((void *)&ultimo_datos, &datos_s, sizeof(datos_slave_t));
        return;
    }
    // otros tamaños: ignorar
    return;
}


// Función para enviar un ping y actualizar el estado de conexión
void estado_conexion_ping(const uint8_t *peer_mac) {
    enviar_estado_conexion(&status_check_MS, peer_mac);
}


// Funcion para enviar un pong en respuesta a un ping recibido
void estado_conexion_pong (const uint8_t *peer_mac) {
    enviar_estado_conexion(&status_check_SM, peer_mac);
}


// Función para verificar el estado de la conexión y actualizar un LED si se proporciona un pin
void ping_pong(const uint8_t *peer_mac, bool *status_conexion, int gpio_led_pin, bool MS) {
    TickType_t tick_actual = xTaskGetTickCount();
    if (MS) {
        if (tiempo_ultimo_ping == 0 || (tick_actual - tiempo_ultimo_ping) >= pdMS_TO_TICKS(5000)) {
            estado_conexion_ping(peer_mac);
            tiempo_ultimo_ping = tick_actual;
        }
    }
    if (status_conexion != NULL) {
        TickType_t ultimo_paquete_esperado = MS ? tiempo_ultimo_pong_rx : tiempo_ultimo_ping_rx;
        if (ultimo_paquete_esperado != 0 && (tick_actual - ultimo_paquete_esperado) <= pdMS_TO_TICKS(5000)) {
            *status_conexion = true;
            if (gpio_led_pin >= 0) {
                gpio_set_level(gpio_led_pin, 1);
            }
        } else {
            *status_conexion = false;
            if (gpio_led_pin >= 0) {
                gpio_set_level(gpio_led_pin, 0);
            }
        }
    }
}


// Función para que la aplicación pueda obtener los últimos datos recibidos de forma segura
void obtener_ultimo_datos(datos_slave_t *out) {
    if (out == NULL) return;
    // Copiar de forma atómica una estructura pequeña
    memcpy(out, (const void *)&ultimo_datos, sizeof(datos_slave_t));
}

// Obtener los últimos datos recibidos desde el maestro
void obtener_ultimo_datos_master(datos_master_t *out) {
    if (out == NULL) return;
    memcpy(out, (const void *)&ultimo_datos_master, sizeof(datos_master_t));
}
