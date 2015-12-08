#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portaudio.h"
#include "util.h"
#include "tables.h"
#include <math.h>

// samplerate = PAL_MAGIC / (2 * period)
#define PAL_MAGIC 7093789.2f

// rendering values
#define SAMP_PERIOD 0x1000000
#define SAMP_PERIOD_BITMASK 0xFFFFFF

static uint32_t master_srate;
static uint32_t master_rate_factor;
static int32_t *outbuffer;
static size_t num_samples;
static PaStream *stream;
static uint8_t num_chn;
static SND_CHANNEL *channels;
static resampling_t res_method;

void init_mixer(uint32_t samplerate, uint8_t num_channels, resampling_t method) {
    if (samplerate % BUF_PER_SEC) {
        fprintf(stderr, "Buffer align won't align with samplerate %d\n", samplerate);
        exit(EXIT_FAILURE);
    }
    master_srate = samplerate;
    num_samples = samplerate / BUF_PER_SEC;
    master_rate_factor = SAMP_PERIOD / master_srate;
    num_chn = num_channels;
    res_method = method;
    outbuffer = xmalloc(sizeof(uint32_t) * num_samples, __FILE__, __LINE__);
    channels = xmalloc(sizeof(SND_CHANNEL) * num_channels, __FILE__, __LINE__);

    // init channels

    for (int i = 0; i < num_channels; i++) {
        channels[i].is_running = false;
        channels[i].volume = MAX_VOLUME;
        channels[i].panpot = 0;
        channels[i].samp_begin_pos = NULL;
        channels[i].current_pos = 0;
        channels[i].current_inter_pos = 0;
        channels[i].loop_start_pos = 0;
        channels[i].loop_end_pos = 0;
        channels[i].end_pos = 0;
        channels[i].finetune = 0;
        channels[i].frequency = 0;
        channels[i].period = 0;
        channels[i].ptarget = 0;
        channels[i].pspeed = 1;
    }

    printf("Buffer allocated at %p\n", outbuffer);
    printf("Num Samples %d\n", (int)num_samples);

    if (Pa_Initialize() != paNoError) {
        fprintf(stderr, "PA Init Error\n");
        exit(EXIT_FAILURE);
    }
    if (Pa_OpenDefaultStream(&stream, 0, 2, paInt16, samplerate, num_samples, NULL, NULL) != paNoError) {
        fprintf(stderr, "PA Open Stream Error\n");
        exit(EXIT_FAILURE);
    }
    if (Pa_StartStream(stream) != paNoError) {
        fprintf(stderr, "Couldn't launch stream\n");
        exit(EXIT_FAILURE);
    }
}

void mixer_render(void) {
    // clear buffer before processing
    memset(outbuffer, 0, num_samples * sizeof(uint32_t));
    for (int chn = 0; chn < num_chn; chn++) {
        if (!channels[chn].is_running || channels[chn].frequency == 0
                || channels[chn].samp_begin_pos == NULL) {
            continue;
        }
        //SMPLDATA *snd_data = channels[chn].samp_begin_pos;
        int vol = channels[chn].volume;
        int pan = channels[chn].panpot;

        int left_vol = 64 - (pan / 2);
        int right_vol = 64 + (pan / 2);
        left_vol = minmax(0, left_vol * vol / 64, 127);
        right_vol = minmax(0, right_vol * vol / 64, 127);

        uint32_t step = master_rate_factor * channels[chn].frequency;

        switch (res_method) {
            case NONE:
                {
                    int32_t *outptr = outbuffer;
                    // vol_lr = 0x00LV00RV
                    int32_t vol_lr = ((right_vol / 2) << 16) + (left_vol / 2);
                    //if (chn == 0) printf("left vol: %d, right vol: %d\n", left_vol, right_vol);

                    for (uint32_t i = 0; i < num_samples; i++) {
                        int8_t input = *(channels[chn].samp_begin_pos + channels[chn].current_pos);
                        *(outptr++) += input * vol_lr;
                        channels[chn].current_inter_pos += step;
                        channels[chn].current_pos += channels[chn].current_inter_pos >> 24;
                        channels[chn].current_inter_pos &= SAMP_PERIOD_BITMASK;
                        if (channels[chn].loop_end_pos > 2) {
                            if (channels[chn].current_pos >= channels[chn].loop_end_pos) {
                                channels[chn].current_pos = channels[chn].loop_start_pos;
                            }
                        } else if (channels[chn].current_pos >= channels[chn].end_pos) {
                            channels[chn].current_pos = 0;
                            channels[chn].current_inter_pos = 0;
                            channels[chn].is_running = false;
                            break;
                        }
                    }
                }
                break; // end case NONE
            case LINEAR:
                {
                    int16_t *outptr = (int16_t *)outbuffer;
                    for (uint32_t i = 0; i < num_samples; i++) {
                        // sample 1
                        int8_t input1 = *(channels[chn].samp_begin_pos + channels[chn].current_pos);
                        int8_t input2 = *(channels[chn].samp_begin_pos + (
                                    (channels[chn].current_pos + 1 >= channels[chn].end_pos) ?
                                    channels[chn].loop_start_pos : channels[chn].current_pos + 1
                                    )
                                );
                        int64_t delta = (input2 - input1) * (int64_t)channels[chn].current_inter_pos;
                        int64_t result = (input1 * SAMP_PERIOD) + delta;
                        int16_t left = (int16_t)((result * left_vol) >> 25);
                        int16_t right = (int16_t)((result * right_vol) >> 25);

                        *outptr = (int16_t)(*outptr + left);
                        outptr++;
                        *outptr = (int16_t)(*outptr + right);
                        outptr++;
                        channels[chn].current_inter_pos += step;
                        channels[chn].current_pos += channels[chn].current_inter_pos >> 24;
                        channels[chn].current_inter_pos &= SAMP_PERIOD_BITMASK;
                        if (channels[chn].loop_end_pos > 2) {
                            if (channels[chn].current_pos >= channels[chn].loop_end_pos) {
                                channels[chn].current_pos = channels[chn].loop_start_pos;
                            }
                        } else if (channels[chn].current_pos >= channels[chn].end_pos) {
                            channels[chn].current_pos = 0;
                            channels[chn].current_inter_pos = 0;
                            channels[chn].is_running = false;
                            break;
                        }
                    }
                }
                break; // end case LINEAR
            default:
                break; // end default
        }
    }
    PaError err;
    if ((err = Pa_WriteStream(stream, outbuffer, num_samples)) != paNoError) {
        fprintf(stderr, "PA Stream Writing error\n");
        fprintf(stderr, "PA Error: %s\n", Pa_GetErrorText(err));
    }
}

void stop_mixer(void) {
    if (Pa_StopStream(stream) != paNoError) {
        fprintf(stderr, "Error while stopping the output stream\n");
        exit(EXIT_FAILURE);
    }
    if (Pa_CloseStream(stream) != paNoError) {
        fprintf(stderr, "Error while closing the audio stream\n");
        exit(EXIT_FAILURE);
    }
    if (Pa_Terminate() != paNoError) {
        fprintf(stderr, "PA Termination error\n");
        exit(EXIT_FAILURE);
    }
    free(outbuffer);
    free(channels);
}

static float calc_freq(uint16_t period, int8_t tune) {
    float base = PAL_MAGIC / (float)(2 * period);
    float result = base * (float) pow(2.0f, (float)tune / (8.0f * 12.0f));
    // debug
    // printf("Period %d and Tune %d results: %fHz\n", period, tune, result);
    return result;
}

void chn_play_note(uint8_t chn) {
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].is_running = true;
    channels[chn].current_pos = 2;
    channels[chn].current_inter_pos = 0;
}

void chn_set_instr(uint8_t chn, SMPLDATA *data, uint32_t loop_start, 
        uint32_t loop_length, uint32_t end, int8_t finetune, uint8_t volume) {
    chn = (uint8_t)min(chn, num_chn-1);
    finetune = minmax(-8, finetune, 7);
    volume = minmax(0, volume, 64);
    //    channels[chn].is_running = false;
    channels[chn].samp_begin_pos = data;
    channels[chn].current_pos = 2;
    channels[chn].current_inter_pos = 0;
    channels[chn].loop_start_pos = loop_start;
    channels[chn].loop_end_pos = loop_length + loop_start;
    channels[chn].end_pos = end;
    channels[chn].finetune = finetune;
    channels[chn].volume = volume;
    // debug
    //if (chn == 0) printf("Starting note with volume %d\n", volume);
}

void chn_set_vol(uint8_t chn, uint8_t vol) {
    chn = (uint8_t)min(chn, num_chn-1);
    // debug
    //    if (chn == 3) printf("CHN%d --> Vol %d\n", chn, vol);
    vol = min(vol, 64);
    channels[chn].volume = vol;
}

void chn_set_finetune(uint8_t chn, int8_t finetune) {
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].finetune = finetune;
    channels[chn].frequency = (uint32_t)calc_freq(channels[chn].period, finetune);
}

void chn_set_period(uint8_t chn, uint16_t period) {
    chn = (uint8_t)min(chn, num_chn-1);
    period = minmax(MIN_PER, period, MAX_PER);
    channels[chn].period = period;
    channels[chn].frequency = (uint32_t)calc_freq(period, channels[chn].finetune);
}

void chn_period_add(uint8_t chn, int amount) {
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].period = (uint16_t)minmax(MIN_PER, channels[chn].period + amount, MAX_PER);
    channels[chn].frequency = (uint32_t)calc_freq(channels[chn].period, channels[chn].finetune);
}

void chn_set_ptarget(uint8_t chn, uint16_t period) {
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].ptarget = minmax(MIN_PER, period, MAX_PER);
}

void chn_slide_p(uint8_t chn, uint8_t amount) {
    chn = (uint8_t)min(chn, num_chn-1);
    // ignore portamento if no note is currently being played
    if (channels[chn].period == 0) return;
    if (amount > 0) channels[chn].pspeed = amount;
    if (channels[chn].ptarget < channels[chn].period) {
        channels[chn].period = (uint16_t)max(channels[chn].ptarget, channels[chn].period - channels[chn].pspeed);
    } else if (channels[chn].ptarget > channels[chn].period) {
        channels[chn].period = (uint16_t)min(channels[chn].ptarget, channels[chn].period + channels[chn].pspeed);
    }
    channels[chn].frequency = (uint32_t)calc_freq(channels[chn].period, channels[chn].finetune);
}

void chn_continue_p(uint8_t chn) {
    chn = (uint8_t)min(chn, num_chn-1);
    if (channels[chn].period == 0) return;
    if (channels[chn].ptarget < channels[chn].period) {
        channels[chn].period = (uint16_t)max(channels[chn].ptarget, channels[chn].period - channels[chn].pspeed);
    } else if (channels[chn].ptarget > channels[chn].period) {
        channels[chn].period = (uint16_t)min(channels[chn].ptarget, channels[chn].period + channels[chn].pspeed);
    }
    channels[chn].frequency = (uint32_t)calc_freq(channels[chn].period, channels[chn].finetune);   
}

void chn_vol_add(uint8_t chn, int amount) {
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].volume = (uint8_t)minmax(0, channels[chn].volume + amount, 64);
    //    if (chn == 3) printf("CHN%d --> Vol %d\n", chn, channels[chn].volume);
}

void chn_set_pan(uint8_t chn, int pos) {
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].panpot = (int8_t) minmax(-128, pos, 127);
}

void chn_seek(uint8_t chn, uint32_t pos) {
    if (pos == 0) return;
    chn = (uint8_t)min(chn, num_chn-1);
    channels[chn].current_pos = pos;
    channels[chn].current_inter_pos = 0;
    channels[chn].is_running = true;
}
