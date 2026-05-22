#ifndef PWM_H
#define PWM_H

#include <stdbool.h>
#include <stdbool.h>


#include "driver/gpio.h"

void configuracion_pwm(gpio_num_t pin, ledc_channel_t canal, ledc_timer_t timer, int frecuencia, ledc_timer_bit_t resolucion_bits, bool *status_pwm_init);
int clamp_int(int *valor, int min, int max);
void setear_duty_cycle(int duty_cycle, bool *status);
void encender_pwm_suave(uint32_t tiempo_ms, uint32_t pasos, bool *status_pwm);
void apagar_pwm_suave(uint32_t tiempo_ms, uint32_t pasos, bool *status_pwm);

#endif 