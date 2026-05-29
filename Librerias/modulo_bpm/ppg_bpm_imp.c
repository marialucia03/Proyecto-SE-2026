#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include "ppg_bpm.h"

/// Variables de configuración y estado del filtro y cálculo de BPM
#define FILTER_SIZE               2
#define BPM_AVG_SIZE              4
#define MIN_INTERVAL_MS           300
#define MAX_INTERVAL_MS           1500
#define SIGNAL_TIMEOUT_MS         2500

// Theshold movil 
static float threshold_base = 0;
static uint16_t last_sample = 0;

// Buffers y variables para el filtro de media móvil y cálculo de BPM
static uint16_t filter_buffer[FILTER_SIZE];
static uint32_t filter_sum = 0;
static uint8_t filter_index = 0;
static uint16_t bpm_buffer[BPM_AVG_SIZE];
static uint32_t bpm_sum = 0;
static uint8_t bpm_index = 0;
static uint32_t last_peak_ms = 0;
static bool peak_armed = true;
static bool signal_ok = false;


// Remover componente DC
static float dc_estimate = 0;

// Filtro Low pass
static float lowpass_state = 0;

// BPM
static uint8_t bpm_valid_samples = 0;
static uint16_t bpm = 0;



static uint16_t filtro_pasabajas(uint16_t entrada)
{
    //-----------------------------------------
    // alpha pequeño = más suavizado
    //-----------------------------------------
    const float alpha = 0.4f;

    lowpass_state =
        lowpass_state +
        alpha * ((float)entrada - lowpass_state);

    return (uint16_t)lowpass_state;
}


static uint16_t threshold_dinamico(uint16_t señal)
{
    //-----------------------------------------
    // baseline lento
    //-----------------------------------------
    threshold_base =
        threshold_base +
        0.03f * ((float)señal - threshold_base);

    //-----------------------------------------
    // margen sobre baseline
    //-----------------------------------------
    return (uint16_t)(threshold_base + 5);
}


static uint16_t promedio_movil(uint16_t muestra){
    filter_sum -= filter_buffer[filter_index];
    filter_buffer[filter_index] = muestra;
    filter_sum += filter_buffer[filter_index];

    filter_index = (filter_index + 1) % FILTER_SIZE;

    return  filter_sum/ FILTER_SIZE;
}


void calculo_bpm(uint16_t muestra,
                 uint32_t tick_muestra)
{

// NO HAY DEDO

if (muestra < 1000)
{
    signal_ok = false;

    bpm = 0;

    peak_armed = true;

    return;
}


// DETECCION MOVIMIENTO

int32_t delta =
    (int32_t)muestra -
    (int32_t)last_sample;

last_sample = muestra;


// MOVIMIENTO REAL

bool movimiento =
    abs(delta) > 12000;


// SI HAY MOVIMIENTO

if (movimiento)
{

    // REARMAR DETECTOR
 
    return;
}


// REMOVER DC

dc_estimate =
    dc_estimate +
    0.05f *
    ((float)muestra - dc_estimate);


// COMPONENTE AC

int16_t ac_signal =
    (int16_t)((float)muestra - dc_estimate);


// HALF-WAVE RECTIFIER

if (ac_signal < 0)
{
    ac_signal = 0;
}

uint16_t magnitud =
    (uint16_t)ac_signal;


// LOW PASS IIR

uint16_t lp =
    filtro_pasabajas(magnitud);


// SEÑAL FILTRADA

uint16_t valor_filtrado = lp;


// THRESHOLD DINAMICO

static uint16_t threshold_high = 0;


// NO ACTUALIZAR THRESHOLD
// DURANTE MOVIMIENTO

if (!movimiento)
{
    threshold_high =
        threshold_dinamico(
            valor_filtrado);
}


// HISTERESIS

uint16_t threshold_low = threshold_high * 0.6f;


// TIMEOUT

if ((tick_muestra - last_peak_ms) >SIGNAL_TIMEOUT_MS)
{
    signal_ok = false;

    bpm = 0;

    memset(bpm_buffer,
           0,
           sizeof(bpm_buffer));

    bpm_sum = 0;

    bpm_valid_samples = 0;

    peak_armed = true;
}

// DETECTOR DE PICOS

if (peak_armed)
{
    uint32_t tiempo_desde_pico =
    tick_muestra - last_peak_ms;

    if (tiempo_desde_pico < 250)
    {
        return;
    }

    if (valor_filtrado >
        threshold_high)
    {

        if (last_peak_ms != 0)
        {
            uint32_t intervalo_ms =
                tick_muestra -
                last_peak_ms;

            if (intervalo_ms >=
                    MIN_INTERVAL_MS &&
                intervalo_ms <=
                    MAX_INTERVAL_MS)
            {
                uint16_t bpm_inst =
                    60000 /
                    intervalo_ms;

                // VALIDACION FISIOLOGICA

                if (bpm > 0)
                {
                    if (abs(
                        (int)bpm_inst -
                        (int)bpm) > 25)
                    {
                        return;
                    }
                }


                // PROMEDIO BPM

                bpm_sum -=
                    bpm_buffer[bpm_index];

                bpm_buffer[bpm_index] =
                    bpm_inst;

                bpm_sum +=
                    bpm_buffer[bpm_index];

                bpm_index =
                    (bpm_index + 1) %
                    BPM_AVG_SIZE;

                if (bpm_valid_samples <
                    BPM_AVG_SIZE)
                {
                    bpm_valid_samples++;
                }


                // BPM SUAVIZADO

                float bpm_nuevo =
                    (float)bpm_sum /
                    bpm_valid_samples;

                bpm =
                    (uint16_t)(
                        0.8f * bpm +
                        0.2f * bpm_nuevo);

                signal_ok = true;
            }
        }

        // GUARDAR PICO

        last_peak_ms =
            tick_muestra;

        peak_armed = false;
    }
}
else
{
    if (valor_filtrado <
        threshold_low)
    {
        peak_armed = true;
    }
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

