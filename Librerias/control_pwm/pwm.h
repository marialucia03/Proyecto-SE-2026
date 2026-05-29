#ifndef PWM_H
#define PWM_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

void configuracion_pwm(gpio_num_t pin, ledc_channel_t canal, ledc_timer_t timer, int frecuencia, ledc_timer_bit_t resolucion_bits, int *status_pwm_init);
void setear_duty_cycle(uint32_t duty, bool *status);
void encender_pwm_suave(uint32_t tiempo_ms, uint32_t pasos, int *status_pwm);
void apagar_pwm_suave(uint32_t tiempo_ms, uint32_t pasos, int *status_pwm);

#endif 
