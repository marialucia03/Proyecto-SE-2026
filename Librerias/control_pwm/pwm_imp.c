#include "pwm.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Variable de configuracion del PWM
#define PWM_SPEED_MODE LEDC_LOW_SPEED_MODE

static ledc_channel_t canal_pwm;
static ledc_timer_t timer_pwm;
static gpio_num_t pin_pwm = GPIO_NUM_NC;
static bool pwm_configurado = false;
static uint32_t duty_maximo = 0;


// Funcion para limitar un valor entero dentro de un rango
static int clamp_int(int valor, int min, int max)
{
    if (valor < min) return min;
    if (valor > max) return max;

    return valor;
}


// Configuracion pwm
void configuracion_pwm(    gpio_num_t pin,    ledc_channel_t canal,    ledc_timer_t timer,    int frecuencia,    ledc_timer_bit_t resolucion_bits,    bool *status_pwm_init)
{
    if (status_pwm_init != NULL) {
        *status_pwm_init = false;
    }
    canal_pwm = canal;
    timer_pwm = timer;
    pin_pwm = pin;


    ledc_timer_config_t timer_conf = {
        .speed_mode = PWM_SPEED_MODE,
        .timer_num = timer_pwm,
        .duty_resolution = resolucion_bits,
        .freq_hz = frecuencia,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t rc = ledc_timer_config(&timer_conf);

    /* Configuracion canal */
    ledc_channel_config_t canal_conf = {
        .gpio_num = pin_pwm,
        .speed_mode = PWM_SPEED_MODE,
        .channel = canal_pwm,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_pwm,
        .duty = 0,
        .hpoint = 0
    };
    rc = ledc_channel_config(&canal_conf);

    duty_maximo = (1 << resolucion_bits) - 1;
    pwm_configurado = true;
    if (status_pwm_init != NULL) {
        *status_pwm_init = true;
}
}


// Funcion para establecer el duty cycle del PWM
void setear_duty_cycle(uint32_t duty, bool *status)
{
    if (status != NULL) {
        *status = false;
    }
    duty = clamp_int(duty, 0, duty_maximo);
    esp_err_t rc = ledc_set_duty(
        PWM_SPEED_MODE,
        canal_pwm,
        duty);
    rc = ledc_update_duty( PWM_SPEED_MODE, canal_pwm);
    if (status != NULL) {
        *status = true;
    }
}


// Encender el PWM de forma suave
void encender_pwm_suave(    uint32_t tiempo_ms,    uint32_t pasos,    bool *status_pwm){
    if (status_pwm != NULL) {
        *status_pwm = false;
    }
    uint32_t delay_ms = tiempo_ms / pasos;
    for (uint32_t i = 0; i <= pasos; i++) {
        uint32_t duty = (i * duty_maximo) / pasos;
        bool status = false;
        setear_duty_cycle(duty, &status);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    if (status_pwm != NULL) {
        *status_pwm = true;
    }
}


// Funcion para apagar el PWM de forma suave
void apagar_pwm_suave(    uint32_t tiempo_ms,    uint32_t pasos,    bool *status_pwm)
{
    if (status_pwm != NULL) {
        *status_pwm = false;
    }
    uint32_t delay_ms = tiempo_ms / pasos;
    for (int i = pasos; i >= 0; i--) {
        uint32_t duty = (i * duty_maximo) / pasos;
        bool status = false;
        setear_duty_cycle(duty, &status);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    if (status_pwm != NULL) {
        *status_pwm = true;
    }
}
