#ifndef DISPLAY_H
#define DISPLAY_H
#include "stdio.h"
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#define DISPLAY_ADDRESS 0x27
#define BL   0x08   // bit 3
#define EN   0x04   // bit 2
#define RW   0x02   // bit 1  
#define RS   0x01   // bit 0
volatile bool resultado_inicializacion = 0;
volatile bool resultado_4_bit_mode = 0;

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