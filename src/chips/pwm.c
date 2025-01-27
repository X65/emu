#include "./pwm.h"

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void pwm_init(pwm_t* pwm, int tick_hz) {
    CHIPS_ASSERT(pwm);
    CHIPS_ASSERT(tick_hz > 0);
    *pwm = (pwm_t){
        .tick_hz = tick_hz,
        .counter = 0,
        .period = 0,
        .duty = 0,
        .new_duty = 0,
    };
}

void pwm_reset(pwm_t* pwm) {
    CHIPS_ASSERT(pwm);
    pwm->counter = pwm->period;
    pwm->duty = 0;
    pwm->new_duty = 0;
}

void pwm_set_freq(pwm_t* pwm, uint16_t freq) {
    pwm->period = freq ? ((pwm->tick_hz * PWM_FIXEDPOINT_SCALE) / freq) : 0;
}

int pwm_get_state(pwm_t* pwm) {
    return pwm->counter < pwm->period * pwm->duty / 255;
}

void pwm_tick(pwm_t* pwm) {
    if (pwm->period) pwm->counter += PWM_FIXEDPOINT_SCALE;
    if (pwm->counter >= pwm->period) {
        pwm->counter -= pwm->period;
        pwm->duty = pwm->new_duty;
    }
}
