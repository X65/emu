#pragma once
/*
    pwm.h    -- Pulse Width Modulator

    ## 0BSD license

    Copyright (c) 2025 Tomasz Sterna
*/
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// error-accumulation precision boost
#define PWM_FIXEDPOINT_SCALE (16)

// PWM state
typedef struct {
    uint tick_hz;
    int counter;
    int period;
    uint8_t duty;
    uint8_t new_duty;
} pwm_t;

// initialize PWM instance
void pwm_init(pwm_t* pwm, int tick_hz);
// reset the PWM instance
void pwm_reset(pwm_t* pwm);

void pwm_set_freq(pwm_t* pwm, uint16_t freq);
static inline void pwm_set_duty(pwm_t* pwm, uint8_t duty) {
    pwm->new_duty = duty;
}

int pwm_get_state(pwm_t* pwm);

// tick the PWM
void pwm_tick(pwm_t* pwm);

#ifdef __cplusplus
} /* extern "C" */
#endif
