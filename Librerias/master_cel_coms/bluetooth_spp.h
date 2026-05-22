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

typedef void (*ble_tx_ready_callback_t)(bool tx_ready, void *ctx);
void bluetooth_init(void);
void ble_send_init_report(const logs_inits_t *data);
void ble_send_runtime_report(const logs_status_t *data);
void nus_send_response(const char *msg);
bool ble_is_tx_ready(void);
void ble_register_tx_ready_callback(ble_tx_ready_callback_t callback, void *ctx);
void ble_send_log_inits(const logs_inits_t *inits);
void ble_send_log_status(const logs_status_t *status);
#ifdef __cplusplus
}
#endif
#endif // BLE_LOG_LIB_H
