#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "driver/gpio.h"


void configuracion_i2c(int I2C_PORT,int PIN_SDA, int PIN_SCL);
void enviar_bytes_i2c(uint8_t hex_data);
void enviar_comando_lcd(uint8_t byte, uint8_t flags);
void inicializar_display();
void lcd_clear();
void lcd_set_cursor(uint8_t fila, uint8_t columna);
void print_pantalla(const char* str);
void pantalla_datos_biometricos(int bpm, int temperatura);
void pantalla_conexion_espnow(bool estado_conexion);
void pantalla_pausa();
bool display_cambio_pantalla_pendiente(void);
uint8_t display_pantalla_solicitada(void);
void display_limpiar_cambio_pantalla(void);
void configuracion_control_display(gpio_num_t pin_b1, gpio_num_t pin_b2, gpio_num_t pin_b3);
int  control_display_actualizar();
#endif
