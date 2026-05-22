#ifndef COMUNICACIONES_H
#define COMUNICACIONES_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"

typedef struct {
    int voltaje_potenciometro;
    int temperatura;
    int bpm;
} datos_t;


void configuracion_wifi(int canal,bool *status_wifi_init);
void configuracion_espnow(int canal, const uint8_t *peer_mac, bool *status_espnow_init);
void enviar_datos(const datos_t *datos, const uint8_t *peer_mac);
void recibir_datos(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
void estado_conexion_ping(const uint8_t *peer_mac);
void estado_conexion_pong (const uint8_t *peer_mac);
void ping_pong(const uint8_t *peer_mac, bool *status_conexion, int gpio_led_pin, bool MS);
// Devuelve una copia del último paquete `datos_t` recibido vía ESP-NOW
void obtener_ultimo_datos(datos_t *out);

#endif

