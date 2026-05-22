#include <string.h>

#include "comunicaciones.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"


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
// Últimos datos recibidos desde el peer
static volatile datos_t ultimo_datos = {0};

static void enviar_estado_conexion(const estado_conexion_t *estado, const uint8_t *peer_mac) {
    esp_now_send(peer_mac, (const uint8_t *)estado, sizeof(*estado));
}

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

void configuracion_espnow(int canal , const uint8_t *peer_mac, bool *status_espnow_init) {
    ESP_ERROR_CHECK(esp_now_init());

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

void enviar_datos(const datos_t *datos, const uint8_t *peer_mac) {
    esp_now_send(peer_mac, (const uint8_t *)datos, sizeof(datos_t));
}

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

    if (len != sizeof(datos_t)) {
        return;
    }

    datos_t datos;
    memcpy(&datos, data, sizeof(datos));
    // Guardar copia de los datos recibidos para que la aplicación los consulte
    memcpy((void *)&ultimo_datos, &datos, sizeof(datos_t));
}

void estado_conexion_ping(const uint8_t *peer_mac) {
    enviar_estado_conexion(&status_check_MS, peer_mac);
}

void estado_conexion_pong (const uint8_t *peer_mac) {
    enviar_estado_conexion(&status_check_SM, peer_mac);
}

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

void obtener_ultimo_datos(datos_t *out) {
    if (out == NULL) return;
    // Copiar de forma atómica una estructura pequeña
    memcpy(out, (const void *)&ultimo_datos, sizeof(datos_t));
}

