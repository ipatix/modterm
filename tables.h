#pragma once

#include <stdint.h>

#define KEY_RANGE 60
#define TUNE_RANGE 16
#define OCTAVE_RANGE 12

#define KEY_NONE 60
#define KEY_INVALID 61

uint8_t getkey_by_per(uint16_t);

extern const char *note_names[];
extern const uint16_t period_table[TUNE_RANGE][KEY_RANGE];
