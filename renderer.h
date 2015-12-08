#pragma once

#include "modstructs.h"
#include "pa_defs.h"
#include "portaudio.h"
#include <stdbool.h>
#include <stdint.h>

#define BUF_PER_SEC 50

#define MIN_PER 54
#define MAX_PER 1814

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

typedef struct {
    float srate;
    uint8_t key;
} PITCH_ENTRY;

typedef enum {
    NONE,
    LINEAR
} resampling_t;

typedef struct {
    SMPLDATA *samp_begin_pos;

    bool is_running;
    uint8_t volume;
    int8_t panpot;
    int8_t finetune;

    uint32_t current_pos;
    uint32_t current_inter_pos;
    uint32_t loop_start_pos;
    uint32_t loop_end_pos;
    uint32_t end_pos;

    uint32_t frequency;
    uint16_t period;
    uint16_t ptarget;
    uint8_t pspeed;
} SND_CHANNEL;

// all functions return true on success, false on failure
void init_mixer(uint32_t, uint8_t, resampling_t);
void mixer_render(void);
void stop_mixer(void);

// channel functions
void chn_play_note(uint8_t);
void chn_set_instr(uint8_t, SMPLDATA *, uint32_t, uint32_t, uint32_t, int8_t, uint8_t);
void chn_set_vol(uint8_t, uint8_t);
void chn_set_finetune(uint8_t, int8_t);
void chn_set_period(uint8_t, uint16_t);
void chn_period_add(uint8_t, int);
void chn_set_ptarget(uint8_t, uint16_t);
void chn_slide_p(uint8_t, uint8_t);
void chn_continue_p(uint8_t);
void chn_vol_add(uint8_t, int);
void chn_set_pan(uint8_t, int);
void chn_seek(uint8_t, uint32_t);
