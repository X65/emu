// Host stand-in for the Pico SDK always-on timer. The emulator implements these
// in src/systems/x65_fw.c on the host wall clock plus an offset, so a guest
// setting the time never touches the host clock.
#pragma once
#include <stdbool.h>
#include <time.h>

bool aon_timer_start(const struct timespec* ts);
bool aon_timer_stop(void);
bool aon_timer_is_running(void);
bool aon_timer_set_time(const struct timespec* ts);
bool aon_timer_get_time(struct timespec* ts);
void aon_timer_get_resolution(struct timespec* ts);
