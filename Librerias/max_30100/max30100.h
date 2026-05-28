#ifndef MAX30100_H
#define MAX30100_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

void configuracion_max30100(bool *status_init);
esp_err_t max30100_read_fifo(uint16_t *ir_value);


#endif 