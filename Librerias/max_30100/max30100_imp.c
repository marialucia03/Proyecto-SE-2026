#include "max30100.h"
#include <stdio.h>
#include <stdbool.h>
#include <driver/i2c.h>
#include "freertos/semphr.h"


#define MAX30100_ADDRESS 0x57 // Dirección I2C del sensor MAX30100
#define I2C_PORT I2C_NUM_0 // Puerto I2C a utilizar
extern SemaphoreHandle_t g_i2c_mutex;
// Funcion enviar bytes
static esp_err_t enviar_bytes_i2c_max30100(uint8_t registro,uint8_t hex_data) {
    if (g_i2c_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, MAX30100_ADDRESS << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, registro, true); // Registro a escribir
    i2c_master_write_byte(cmd, hex_data, true); // Hex data  byte
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_i2c_mutex);

    return ret;
}



// Función para configurar el sensor MAX30100
void configuracion_max30100(bool *status_init) {
    esp_err_t ret = ESP_OK;

    ret = enviar_bytes_i2c_max30100(0x06, 0x40); // Reset de configuracion del sensor
    vTaskDelay(pdMS_TO_TICKS(100)); // Espera a que el sensor se configure

    //Reiniciar FIFO
    if (ret == ESP_OK) ret = enviar_bytes_i2c_max30100(0x02, 0x00);
    if (ret == ESP_OK) ret = enviar_bytes_i2c_max30100(0x03, 0x00);
    if (ret == ESP_OK) ret = enviar_bytes_i2c_max30100(0x04, 0x00); 
    
    // Configurar el sensor para medir el ritmo cardíaco
    if (ret == ESP_OK) ret = enviar_bytes_i2c_max30100(0x07, 0x47); // Configuración del modo de operación (Heart Rate mode)
    if (ret == ESP_OK) ret = enviar_bytes_i2c_max30100(0x09, 0x09); // Configuración del LED rojo (Red LED current)
    if (ret == ESP_OK) ret = enviar_bytes_i2c_max30100(0x06,0x02);

    if (status_init != NULL) {
        *status_init = (ret == ESP_OK); // Indicar que la configuración se ha completado
    }
}


esp_err_t max30100_read_fifo(uint16_t *ir_value)
{
    uint8_t data[4];

    if (g_i2c_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,(MAX30100_ADDRESS << 1) | I2C_MASTER_WRITE,true);
    i2c_master_write_byte(cmd, 0x05, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,(MAX30100_ADDRESS << 1) | I2C_MASTER_READ,true);
    i2c_master_read(cmd, data, 4, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT,cmd,pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(g_i2c_mutex);

    if (ret == ESP_OK)
    {
        *ir_value =
            ((uint16_t)data[0] << 8) |
             data[1];
    }

    return ret;
}







