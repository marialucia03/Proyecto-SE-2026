#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include "ppg_bpm.h"

#define FILTER_SIZE               4
#define BPM_AVG_SIZE              4

#define THRESHOLD_HIGH            200
#define THRESHOLD_LOW             100

#define MIN_INTERVAL_MS           300
#define MAX_INTERVAL_MS           1500

#define SIGNAL_TIMEOUT_MS         2500

static uint16_t filter_buffer[FILTER_SIZE];
static uint32_t filter_sum = 0;
static uint8_t filter_index = 0;

static uint16_t bpm_buffer[BPM_AVG_SIZE];
static uint32_t bpm_sum = 0;
static uint8_t bpm_index = 0;

static uint32_t last_peak_ms = 0;

static uint16_t bpm = 0;

static bool peak_armed = true;
static bool signal_ok = false;

static SemaphoreHandle_t bpm_mutex = NULL;

static uint16_t promedio_movil(uint16_t muestra){
    filter_sum -= filter_buffer[filter_index];
    filter_buffer[filter_index] = muestra;
    filter_sum += filter_buffer[filter_index];

    filter_index = (filter_index + 1) % FILTER_SIZE;

    return  filter_sum >> 2;
}


void calculo_bpm(uint16_t muestra, uint32_t tick_muestra){
    uint16_t valor_filtrado = promedio_movil(muestra);
    if ((tick_muestra)-last_peak_ms > SIGNAL_TIMEOUT_MS){
        signal_ok = false;
        bpm=0;
    }
    if (peak_armed){
        if (valor_filtrado>THRESHOLD_HIGH){
            uint32_t intervalo_ms = tick_muestra - last_peak_ms;
            if (intervalo_ms >= MIN_INTERVAL_MS && intervalo_ms <= MAX_INTERVAL_MS){
                uint16_t bpm_instantaneo = 60000 / intervalo_ms;

                bpm_sum -= bpm_buffer[bpm_index];
                bpm_buffer[bpm_index] = bpm_instantaneo;
                bpm_sum += bpm_buffer[bpm_index];

                bpm_index = (bpm_index + 1) % BPM_AVG_SIZE;

                bpm = bpm_sum >> 2;


                signal_ok = true;
                last_peak_ms = tick_muestra;
            }

            peak_armed = false;
        }
    }
    else if (valor_filtrado < THRESHOLD_LOW){
            peak_armed = true;
    }
    
}

uint16_t ppg_bpm_get(void)
{
    return bpm;
}

bool ppg_bpm_signal_ok(void)
{
    return signal_ok;
}

