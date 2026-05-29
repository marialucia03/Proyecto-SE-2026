#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"


void configuracion_i2c(int I2C_PORT,int PIN_SDA, int PIN_SCL, int *status_i2c_init);
void enviar_bytes_i2c(uint8_t hex_data);
void enviar_comando_lcd(uint8_t byte, uint8_t flags);
void inicializar_display(bool *status_display);
bool display_status_ok(void);
void lcd_clear();
void lcd_set_cursor(uint8_t fila, uint8_t columna);
void print_pantalla(const char* str);
void pantalla_datos_biometricos(int bpm, int temperatura, bool pwm_on);
void pantalla_conexion_espnow(bool estado_conexion);
void pantalla_pausa();
bool display_en_pausa(void);
bool display_apagado(void);
void display_encender(void);
void display_apagar(void);
bool display_cambio_pantalla_pendiente(void);
uint8_t display_pantalla_solicitada(void);
void display_limpiar_cambio_pantalla(void);
void configuracion_control_display(gpio_num_t pin_b1, gpio_num_t pin_b2, gpio_num_t pin_b3, gpio_num_t pin_pwm_override);
int  control_display_actualizar();
bool display_pwm_override_pending(void);
bool display_get_pwm_override(void);
void display_clear_pwm_override(void);
void display_forzar_pwm_off(void);
#endif
