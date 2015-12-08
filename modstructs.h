#pragma once

#include <unistd.h>
#include <stdint.h>

typedef struct {
    const char *filename;
    void *memaddr;
    size_t dsize;
} FILEDESC;

typedef struct {
    char name[22];
    uint8_t length[2];  // big endian word, divied by 2
    uint8_t finetune;
    uint8_t volume;
    uint8_t loop_start[2]; // big endian word, divied by 2
    uint8_t loop_length[2]; // big endian word, divied by 2
} SAMPLE_HEADER;

typedef struct {
    uint8_t data[4];
} NOTE;

typedef struct {
    char songname[20];
    SAMPLE_HEADER smpl_headers[31];
    uint8_t num_patterns;
    uint8_t songend_loop_pos;
    uint8_t patt_table[128];
    char fileformat[4];
    NOTE notes[];
} MOD_HEADER;

typedef int8_t SMPLDATA;

// some random defines

#define PATT_SEQ_LEN 0x80
#define NUM_LINES 64
#define NUM_SAMPLES 31
#define MAX_VOLUME 64

// macros to fetch note parameters

#define NOTE_INSTR_NUM(d) ((uint8_t)((d[0]&0xF0)|(d[2]>>4)))
#define NOTE_PER(d) ((uint16_t)((((d[0]&0xF)<<8)|d[1])))
#define NOTE_EFF_CMD(d) ((uint8_t)(d[2]&0xF))
#define NOTE_EFF_DATA(d) d[3]
#define NOTE_EFF_D_HI(d) ((uint8_t)(d[3]>>4))
#define NOTE_EFF_D_LO(d) ((uint8_t)(d[3]&0xF))

// effect type definition

#define EFF_ARP 0x0
#define EFF_SLIDE_UP 0x1
#define EFF_SLIDE_DOWN 0x2
#define EFF_PORTAMENTO 0x3
#define EFF_VIBRATO 0x4
#define EFF_PORT_VS 0x5
#define EFF_VIB_VS 0x6
#define EFF_TREMOLO 0x7
#define EFF_PANPOT 0x8
#define EFF_SAMP_SEEK 0x9
#define EFF_VS 0xA
#define EFF_JUMP 0xB
#define EFF_VOLUME 0xC
#define EFF_BREAK 0xD
#define EFF_EXTENDED 0xE
#define EFF_TEMPO 0xF

// extended effect definintions

#define EFF_EX_FILTER 0x0
#define EFF_EX_F_SLIDE_UP 0x1
#define EFF_EX_F_SLIDE_DOWN 0x2
#define EFF_EX_GLISS 0x3
#define EFF_EX_VIB_TYPE 0x4
#define EFF_EX_FINE_TUNE 0x5
#define EFF_EX_SET_JUMP 0x6
#define EFF_EX_TREM_TYPE 0x7
// 0x8 unused
#define EFF_EX_RETR_NOTE 0x9
#define EFF_EX_F_VOL_UP 0xA
#define EFF_EX_F_VOL_DOWN 0xB
#define EFF_EX_NOTE_CUT 0xC
#define EFF_EX_NOTE_DEL 0xD
#define EFF_EX_PATT_DEL 0xE
#define EFF_EX_INV_LOOP 0xF

