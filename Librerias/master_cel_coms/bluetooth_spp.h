#ifndef BLUETOOTH_SPP_H
#define BLUETOOTH_SPP_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool status_wifi_init;
    bool status_espnow_init;
    bool status_adc_init;
    bool status_i2c_init;
    bool status_pwm_init;
} logs_inits_t;

typedef struct {
    bool status_pwm;
    bool status_conexion;
    bool status_lectura;
} logs_status_t;

/* --- Estructuras de Datos de Logs --- */


/* --- API de Comunicación --- */

/**
 * @brief Inicializa el stack BLE, servicios GATT (NUS) y comienza el advertising.
 * @note Requiere que nvs_flash_init() haya sido llamado previamente.
 */
void bluetooth_init(void);

/**
 * @brief Formatea y envía un reporte de inicialización a la tablet.
 * @param data Puntero a la estructura de inicialización.
 */
void ble_send_init_report(const logs_inits_t *data);

/**
 * @brief Formatea y envía un reporte de estado en tiempo real.
 * @param data Puntero a la estructura de estado operativo.
 */
void ble_send_runtime_report(const logs_status_t *data);

/**
 * @brief Envía un mensaje de texto plano genérico vía BLE.
 * @param msg Cadena de caracteres terminada en nulo.
 */
void nus_send_response(const char *msg);

void ble_send_log_inits(const logs_inits_t *inits);
void ble_send_log_status(const logs_status_t *status);
#ifdef __cplusplus
}
#endif

#endif // BLE_LOG_LIB_H