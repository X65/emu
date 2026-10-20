// Host stand-in for the Pico SDK header. The emulator implements get_rand_32()
// in src/systems/x65_fw.c on top of the same rand() the RIA HW_RNG register
// uses, so --seed covers both.
#pragma once
#include <stdint.h>

uint32_t get_rand_32(void);
