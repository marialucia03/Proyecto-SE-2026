#ifndef PPG_BPM_H
#define PPG_BPM_H
#include <stddef.h>
#include <stdio.h>


void calculo_bpm(uint16_t muestra, uint32_t tick_muestra);
uint16_t ppg_bpm_get(void);
bool ppg_bpm_signal_ok(void);

#endif