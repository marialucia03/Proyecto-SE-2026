#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include <stdarg.h>


/// Variables y definiciones para el control del display LCD 16x2 con I2C

#define DISPLAY_ADDRESS 0x27
#define BL   0x08   // bit 3
#define EN   0x04   // bit 2
#define RW   0x02   // bit 1  
#define RS   0x01   // bit 0

static i2c_port_t s_i2c_port = I2C_NUM_0;
static int pantalla_actual = -1; // 0: datos biométricos, 1: conexión ESP-NOW, 2: pausa  
static volatile bool s_cambio_pantalla_pendiente = false;
static volatile uint8_t s_pantalla_solicitada = 255;

#define PANTALLA_BIOMETRICOS 0
#define PANTALLA_CONEXION    1
#define PANTALLA_PAUSA       2
#define PANTALLA_INVALIDA    255

/// Funciones para el control del display LCD 16x2 con I2C

//Configuración del I2C para el LCD
void configuracion_i2c(int I2C_PORT,int PIN_SDA, int PIN_SCL) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    s_i2c_port = (i2c_port_t)I2C_PORT;
}


// Funciones para enviar comandos  por i2c
void enviar_bytes_i2c(uint8_t hex_data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DISPLAY_ADDRESS << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, hex_data, true); // Hex data  byte
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

}


// Funciones para enviar comandos y datos al LCD, con el formato requerido por el PCF8574
void enviar_al_lcd(uint8_t byte, uint8_t flags){
    
    uint8_t nibble_alto = (byte & 0xF0);        // toma los 4 bits altos
    uint8_t nibble_bajo = (byte & 0x0F) << 4;   // toma los 4 bits bajos y los sube

    // nibble alto con pulso EN
    enviar_bytes_i2c(nibble_alto | BL | EN | flags);
    esp_rom_delay_us(1);
    enviar_bytes_i2c(nibble_alto | BL      | flags);
    esp_rom_delay_us(50);

    // nibble bajo con pulso EN
    enviar_bytes_i2c(nibble_bajo | BL | EN | flags);
    esp_rom_delay_us(1);
    enviar_bytes_i2c(nibble_bajo | BL      | flags);
    esp_rom_delay_us(50);
}


// Funcion para inicializar el display 
void inicializar_display(){
    vTaskDelay(pdMS_TO_TICKS(50)); // Espera a que el LCD esté listo
    enviar_bytes_i2c(0x30 | BL | EN);   // 0x3 en posición alta, EN=1
    esp_rom_delay_us(100);
    enviar_bytes_i2c(0x30 | BL);        // EN=0, HD44780 captura
    vTaskDelay(pdMS_TO_TICKS(5));

    enviar_bytes_i2c(0x30 | BL | EN );
    esp_rom_delay_us(100);
    enviar_bytes_i2c(0x30 | BL );
    esp_rom_delay_us(150);

    enviar_bytes_i2c(0x30 | BL | EN);
    esp_rom_delay_us(100);
    enviar_bytes_i2c(0x30 | BL);
    esp_rom_delay_us(150);

    enviar_bytes_i2c(0x20 | BL | EN);
    esp_rom_delay_us(100);
    enviar_bytes_i2c(0x20 | BL);
    esp_rom_delay_us(150);

    enviar_al_lcd(0x28, 0x00);          // function set: 4 bits, 2 líneas, 5x8
    enviar_al_lcd(0x0C, 0x00);          // display on, cursor off, blink off
    enviar_al_lcd(0x01, 0x00);          // clear display
    vTaskDelay(pdMS_TO_TICKS(2));       
    enviar_al_lcd(0x06, 0x00);          // entry mode: cursor avanza derecha
}


// Funcion para limpiar el display
void lcd_clear() {
    enviar_al_lcd(0x01, 0x00);
    vTaskDelay(pdMS_TO_TICKS(2));
}


// Funcion para posicionar el cursor en la fila y columna deseada (0-indexado)
void lcd_set_cursor(uint8_t fila, uint8_t columna) {

    uint8_t direccion;
    switch(fila) {
        case 0:
            direccion = 0x00 + columna;
            break;

        case 1:
            direccion = 0x40 + columna;
            break;

        default:
            direccion = 0x00 + columna;
            break;
    }
    enviar_al_lcd(0x80 | direccion, 0x00);
}


// Función para imprimir texto formateado en el LCD, con soporte para el símbolo de grados (°) usando un código especial
void print_pantalla(const char* formato, ...) {
    char buffer[33];  // máximo 32 caracteres del display + null terminator
    va_list args;
    va_start(args, formato);
    vsnprintf(buffer, sizeof(buffer), formato, args);
    va_end(args);
    // enviar cada carácter
    char* ptr = buffer;
    while (*ptr) {
        if (*ptr == '\x01') {          // código especial para °
            enviar_al_lcd(0xDF, RS);
        } else {
            enviar_al_lcd((uint8_t)*ptr, RS);
        }
        ptr++;
    }
}


// Funcion de para mostrar datos biometricos
void pantalla_datos_biometricos(int bpm, int temperatura) {
    if (pantalla_actual != 0) {
    enviar_al_lcd(0x01, 0x00);
    vTaskDelay(pdMS_TO_TICKS(2));

    enviar_al_lcd(0x80, 0x00);        // inicio línea 1
    print_pantalla("BPM:    ");            // texto fijo con espacios reservados

    enviar_al_lcd(0x80|0x40, 0x00);  // inicio línea 2
    print_pantalla("T:    \x01C");
    pantalla_actual = 0;
    }
    enviar_al_lcd(0x80 | 0x05, 0x00);    // línea 1 columna 5
    print_pantalla("%3d", bpm);

    enviar_al_lcd(0x80 | 0x42, 0x00);    // línea 2 columna 2
    print_pantalla("%3d \x01C", temperatura);
}


// Función para mostrar el estado de la conexión ESP-NOW
void pantalla_conexion_espnow(bool estado_conexion) {
    if (pantalla_actual != 1) {
        enviar_al_lcd(0x01, 0x00);
        vTaskDelay(pdMS_TO_TICKS(2));

        enviar_al_lcd(0x80, 0x00);        // inicio línea 1
        print_pantalla("ESP-NOW:");       // texto fijo

        enviar_al_lcd(0x80|0x40, 0x00);  // inicio línea 2
        print_pantalla("Estado:       "); // texto fijo con espacios reservados
        pantalla_actual = 1;
    }
    enviar_al_lcd(0x80 | 0x4B, 0x00);    // línea 2 columna 11
    print_pantalla("%-4s", estado_conexion ? "OK" : "FAIL");
}


// Función para mostrar la pantalla de pausa
void pantalla_pausa() {
    if (pantalla_actual != 2) {
        enviar_al_lcd(0x01, 0x00);
        vTaskDelay(pdMS_TO_TICKS(2));

        enviar_al_lcd(0x80, 0x00);        // inicio línea 1
        print_pantalla("PAUSA");           // texto fijo

        enviar_al_lcd(0x80|0x40, 0x00);  // inicio línea 2
        print_pantalla("Presione botón");   // texto fijo
        pantalla_actual = 2;
    }
}


// Funciones para el control de los botones físicos y la gestión de cambios de pantalla
static void IRAM_ATTR atender_boton_isr(gpio_num_t pin_boton, volatile TickType_t *ultimo_disparo, uint8_t pantalla_objetivo)
{
    TickType_t tiempo_actual = xTaskGetTickCountFromISR();

    if ((tiempo_actual - *ultimo_disparo) > pdMS_TO_TICKS(200) && gpio_get_level(pin_boton) == 0) {
        *ultimo_disparo = tiempo_actual;
        s_pantalla_solicitada = pantalla_objetivo;
        s_cambio_pantalla_pendiente = true;
    }
}


// Funciones de interrupcion para los 3 botones
static void IRAM_ATTR  leer_boton_b1(void *arg) {
    static volatile TickType_t ultimo_disparo_b1 = 0;
    atender_boton_isr((gpio_num_t)(uintptr_t)arg, &ultimo_disparo_b1, PANTALLA_BIOMETRICOS);
}


static void IRAM_ATTR  leer_boton_b2(void *arg) {
    static volatile TickType_t ultimo_disparo_b2 = 0;
    atender_boton_isr((gpio_num_t)(uintptr_t)arg, &ultimo_disparo_b2, PANTALLA_CONEXION);
}


static void IRAM_ATTR leer_boton_b3(void *arg) {
    static volatile TickType_t ultimo_disparo_b3 = 0;
    atender_boton_isr((gpio_num_t)(uintptr_t)arg, &ultimo_disparo_b3, PANTALLA_PAUSA);
}


// Funcion auxiliar para consultar si hay cambios de pantall pendientes
bool display_cambio_pantalla_pendiente(void) {
    return s_cambio_pantalla_pendiente;
}


// Funcion auxiliar para obtener la pantalla solicitada por el usuario a través de los botones físicos
uint8_t display_pantalla_solicitada(void) {
    return s_pantalla_solicitada;
}


// Función para limpiar el estado de cambio de pantalla pendiente, que debe ser llamada después de procesar el cambio solicitado
void display_limpiar_cambio_pantalla(void) {
    s_cambio_pantalla_pendiente = false;
    s_pantalla_solicitada = PANTALLA_INVALIDA;
}


// Función para configurar los botones físicos con interrupciones
void configuracion_control_display(gpio_num_t pin_b1, gpio_num_t pin_b2, gpio_num_t pin_b3) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << pin_b1) | (1ULL << pin_b2) | (1ULL << pin_b3),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0); // Sin pull-up ni pull-down, sin interrupciones por defecto
    gpio_isr_handler_add(pin_b1, leer_boton_b1, (void *)(uintptr_t)pin_b1); // Interrupción para botón B1
    gpio_isr_handler_add(pin_b2, leer_boton_b2, (void *)(uintptr_t)pin_b2); // Interrupción para botón B2
    gpio_isr_handler_add(pin_b3, leer_boton_b3, (void *)(uintptr_t)pin_b3); // Interrupción para botón B3
}


// Funcion para actualizar la pantalla segun la solicitud del usuario.
int  control_display_actualizar() {
        switch (s_pantalla_solicitada) {
            case PANTALLA_BIOMETRICOS:
                pantalla_actual=0;
                break;
            case PANTALLA_CONEXION:
                pantalla_actual=1;// Estado inicial, se actualizará luego
                break;
            case PANTALLA_PAUSA:
                pantalla_actual=2;
                break;
            default:
                break;
        }
        return pantalla_actual;
        display_limpiar_cambio_pantalla();
}

