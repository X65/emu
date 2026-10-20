// Host stand-in for the Pico SDK header. The emulator implements time_us_64()
// in src/systems/x65_fw.c from the RIA's emulated microsecond clock.
#pragma once
#include <stdint.h>

uint64_t time_us_64(void);
