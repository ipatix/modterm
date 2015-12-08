#include "modstructs.h"
#include "player.h"
#include "renderer.h"
#include "stdbool.h"
#include "tables.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int read_u16_be(void *data) {
    return ((uint8_t *)data)[0] << 8 | ( ((uint8_t *)data)[1]);
}

static int8_t sl_nibble(uint8_t byte) {
    return (int8_t)(byte << 28 >> 28);
}

static void print_samp_dbg(SAMPLE_HEADER headers[], int numinstr) {
    (void) headers;
    (void) numinstr;
    /*for (int i = 0; i < numinstr; i++) {
        printf("Sample %d -> length: %d, volume: %d, loop start: %d, loop length: %d\n", 
                i+1,
                read_u16_be(headers[i].length)*2,
                headers[i].volume,
                read_u16_be(headers[i].loop_start)*2,
                read_u16_be(headers[i].loop_length)*2
              );
    }*/
}

static int get_num_channels(const char *type) {
    if (!strncmp(type, "M.K.", 4) || !strncmp(type, "CHN4", 4) || !strncmp(type, "FLT4", 4)) {
        return 4;
    }
    if (!strncmp(type, "CHN6", 4)) {
        return 6;
    }
    if (!strncmp(type, "CHN8", 4) || !strncmp(type, "FLT8", 4)) {
        return 8;
    }
    if (!strncmp((type + 2), "CH", 2)) {
        char digits[3];
        strncpy(digits, type, 2);
        digits[2] = '\0';
        int count = atoi(digits);
        if (count == 0) return -1;
        if (count % 2 != 0) return -1;
        return count;
    }
    return -1;
}

static int get_num_patterns(uint8_t data[]) {
    uint8_t maxval = 0;
    for (int i = 0; i < PATT_SEQ_LEN; i++) {
        if (data[i] > maxval) maxval = data[i];
    }
    return maxval + 1;
}

bool play_mod(FILEDESC *fileinfo) {
    // get fileinfo and structs for mod file
    MOD_HEADER *header = (MOD_HEADER *) fileinfo->memaddr;

    int num_channels = get_num_channels(header->fileformat);
    if (num_channels < 0) {
        fprintf(stderr, "Invalid or unsupported MOD format\n");
        exit(EXIT_FAILURE);
    }

    int num_patterns = get_num_patterns(header->patt_table);

    // create new string with \0 termination
    char song_name[sizeof(header->songname) + 1];
    song_name[sizeof(header->songname)] = '\0';
    memcpy(song_name, header->songname, sizeof(header->songname));
    printf("Playing Song \"%s\"!\n", song_name);
    char mod_type[sizeof(header->fileformat) + 1];
    mod_type[sizeof(header->fileformat)] = '\0';
    memcpy(mod_type, header->fileformat, sizeof(header->fileformat));

    // some debugging information
    printf("Module type: %s\n", mod_type);
    printf("Channel amount: %d\n", num_channels);
    printf("Number of patterns: %d\n", num_patterns);

    /*printf("+------ Samples ------+\n");
    for (int i = 0; i < NUM_SAMPLES; i++) {
        size_t tmp = sizeof(header->smpl_headers[i].name);
        char smpl_name[tmp+1];
        smpl_name[tmp] = '\0';
        memcpy(smpl_name, header->smpl_headers[i].name, tmp);
        printf("%d: %s\n", i + 1, smpl_name);
    }*/

    print_samp_dbg(header->smpl_headers, NUM_SAMPLES);

    // get pointer to all samples

    SMPLDATA *sample_data[NUM_SAMPLES];
    SMPLDATA *curr_ptr = (SMPLDATA *)(header->notes + (num_patterns * 64 * num_channels)); // 64 lines per pattern and n notes per line

    for (int i = 0; i < NUM_SAMPLES; i++) {
        sample_data[i] = curr_ptr;
        //printf("Sample %d Pointer: %p\n", i, curr_ptr);
        curr_ptr += (read_u16_be(header->smpl_headers[i].length) * 2);
    }

    // init audio interface for playback

#ifndef _MIXER_LINEAR
    init_mixer(48000, (uint8_t)num_channels, NONE);
#else
    init_mixer(48000, (uint8_t)num_channels, LINEAR);
#endif

    NOTE current_line[num_channels];

    int loop_start = -1;
    int loop_count = -1;
    int speed = 6;
    unsigned long long total_ticks = 0;

    // for all patterns
    for (int c_patt = 0; c_patt < num_patterns; c_patt++) {
        // for all lines
        printf(KNRM "----------------------------------------------\n");
        for (int c_line = 0; c_line < NUM_LINES; c_line++) {
line_start:
            printf(KMAG "P%03d" KNRM "/" KBLU "L%02d" KNRM ": ", c_patt, c_line);
            bool stop_next = false;
            bool line_break = false;
            int delay_fac = 1;
           // read all notes
            for (int c_note = 0; c_note < num_channels; c_note++) {
                current_line[c_note] = header->notes[ 
                    (header->patt_table[c_patt] * NUM_LINES * num_channels) +
                    (c_line * num_channels) +
                    (c_note)
                ];
                /*printf(KNRM "%02X%02X%02X%02X ", 
                        current_line[c_note].data[0], current_line[c_note].data[1],
                        current_line[c_note].data[2], current_line[c_note].data[3]
                        );*/
                printf(KGRN "%s" KYEL "%02X" KRED "%01X" KCYN "%02X ", 
                        note_names[ getkey_by_per(NOTE_PER( current_line[c_note].data )) ],
                        NOTE_INSTR_NUM(current_line[c_note].data),
                        NOTE_EFF_CMD(current_line[c_note].data),
                        NOTE_EFF_DATA(current_line[c_note].data)
                      );
            }
            printf(KNRM "\n");
            // process all ticks per line
            for (int tick = 0; tick < (speed * delay_fac); tick++) {
                // go through all notes
                for (uint8_t c_note = 0; c_note < num_channels; c_note++) {
                    // note handling
                    uint16_t period;
                    int tmp;
                    // check if portamento is used in this effect
                    // if so the note initiation will be a bit different and in the effect itself
                    period = NOTE_PER(current_line[c_note].data);
                    int instr;
                    if (tick == 0 && (instr = NOTE_INSTR_NUM(current_line[c_note].data))) {
                        // period != 0 --> play new note
                        instr = min(instr, NUM_SAMPLES) - 1;
                        SAMPLE_HEADER *smpl = &(header->smpl_headers[instr]);
                        //printf("playing instr #%d ", instr);
                        chn_set_instr((uint8_t)c_note, sample_data[instr],
                                (uint32_t)(read_u16_be(smpl->loop_start) * 2),
                                (uint32_t)(read_u16_be(smpl->loop_length) * 2),
                                (uint32_t)(read_u16_be(smpl->length) * 2),
                                sl_nibble(smpl->finetune),
                                smpl->volume
                                );
                    }
                    if (NOTE_EFF_CMD(current_line[c_note].data) != EFF_PORTAMENTO &&
                            NOTE_EFF_CMD(current_line[c_note].data) != EFF_PORT_VS && tick == 0 && period != 0) {
                        chn_set_period(c_note, period);
                        chn_play_note(c_note);
                    }

                    // effect handling
                    switch (NOTE_EFF_CMD(current_line[c_note].data)) {
                        case EFF_ARP:
                            // TODO
                            break;
                        case EFF_SLIDE_UP: 
                            chn_period_add(c_note, -NOTE_EFF_DATA(current_line[c_note].data));
                            break;
                        case EFF_SLIDE_DOWN:
                            chn_period_add(c_note, NOTE_EFF_DATA(current_line[c_note].data));
                            break;
                        case EFF_PORTAMENTO:
                            if (tick == 0 && period) {
                                chn_set_ptarget(c_note, period);
                                // printf("Set slide target\n");
                            }
                            chn_slide_p(c_note, NOTE_EFF_DATA(current_line[c_note].data));
                            //if (tick == 0 && period ) chn_set_period(c_note, period);
                            break;
                        case EFF_VIBRATO:
                            // TODO
                            break;
                        case EFF_PORT_VS:
                            if (tick == 0 && period) {
                                chn_set_ptarget(c_note, period);
                            }
                            chn_continue_p(c_note);
                            if ((tmp = NOTE_EFF_D_HI(current_line[c_note].data)) > 0) {
                                chn_vol_add(c_note, tmp);
                            }
                            else if ((tmp = NOTE_EFF_D_LO(current_line[c_note].data)) > 0) {
                                chn_vol_add(c_note, -tmp);
                            }
                            break;
                        case EFF_VIB_VS:
                            // TODO vibrato part
                            if ((tmp = NOTE_EFF_D_HI(current_line[c_note].data)) > 0) {
                                chn_vol_add(c_note, tmp);
                            }
                            else if ((tmp = NOTE_EFF_D_LO(current_line[c_note].data)) > 0) {
                                chn_vol_add(c_note, -tmp);
                            }
                            break;
                        case EFF_TREMOLO:
                            // TODO
                            break;
                        case EFF_PANPOT:
                            if (tick == 0) chn_set_pan(c_note, NOTE_EFF_DATA(current_line[c_note].data) - 128);
                            break;
                        case EFF_SAMP_SEEK:
                            if (tick == 0) chn_seek(c_note, (uint32_t)(NOTE_EFF_DATA(current_line[c_note].data) << 8));
                            break;
                        case EFF_VS:
                            if ((tmp = NOTE_EFF_D_HI(current_line[c_note].data)) > 0) {
                                chn_vol_add(c_note, tmp);
                            }
                            else if ((tmp = NOTE_EFF_D_LO(current_line[c_note].data)) > 0) {
                                chn_vol_add(c_note, -tmp);
                            }
                            break;
                        case EFF_JUMP:
                            if (tick == 0) {
                                tmp = NOTE_EFF_DATA(current_line[c_note].data);
                                if (tmp >= num_patterns || tmp < 0) {
                                    fprintf(stderr, "Invalid Pattern Jump\n");
                                    stop_next = true;
                                } else {
                                    line_break = true;
                                    c_patt = tmp;
                                    c_line = 0;
                                }
                            }
                            break;
                        case EFF_VOLUME:
                            if (tick == 0) chn_set_vol(c_note, NOTE_EFF_DATA(current_line[c_note].data));
                            break;
                        case EFF_BREAK:
                            if (tick == 0 && !line_break) {
                                c_patt++;
                                c_line = minmax(0, NOTE_EFF_D_HI(current_line[c_note].data) * 10 + 
                                        NOTE_EFF_D_LO(current_line[c_note].data) - 1, 63);
                                line_break = true;
                                //printf("Jumping to line %d\n", c_line);
                            }
                            break;
                        case EFF_EXTENDED:
                            // extended effect handling
                            switch (NOTE_EFF_D_HI(current_line[c_note].data)) {
                                case EFF_EX_FILTER:
                                    // TODO
                                    // ignore for now, not very important
                                    break;
                                case EFF_EX_F_SLIDE_UP:
                                    if (tick == 0) {
                                        chn_period_add(c_note, -NOTE_EFF_D_LO(current_line[c_note].data));
                                    }
                                    break;
                                case EFF_EX_F_SLIDE_DOWN:
                                    if (tick == 0) {
                                        chn_period_add(c_note, NOTE_EFF_D_LO(current_line[c_note].data));
                                    }
                                    break;
                                case EFF_EX_GLISS:
                                    // TODO
                                    // might be complicated to do
                                    break;
                                case EFF_EX_VIB_TYPE:
                                    // TODO
                                    break;
                                case EFF_EX_FINE_TUNE:
                                    if (tick == 0) {
                                        chn_set_finetune(c_note, sl_nibble(NOTE_EFF_D_LO(current_line[c_note].data)));
                                    }
                                    break;
                                case EFF_EX_SET_JUMP:
                                    if (tick > 0) break;
                                    tmp = NOTE_EFF_D_LO(current_line[c_note].data);
                                    if (tmp == 0) {
                                        loop_start = c_line;
                                    }
                                    else {
                                        if (loop_start < 0) {
                                            fprintf(stderr, "Trying to loop without loop start point\n");
                                        } else {
                                            if (loop_count < 0) {
                                                loop_count = tmp;
                                                c_line = loop_start;
                                                line_break = true;
                                            } else {
                                                if (--loop_count > 0) {
                                                    c_line = loop_start;
                                                    line_break = true;
                                                } else {
                                                    loop_count = -1;
                                                }
                                            }
                                        }
                                    }
                                    break;
                                case EFF_EX_TREM_TYPE:
                                    // TODO
                                    break;
                                case 0x8:
                                    // unused effect, doesn't do anything
                                    break;
                                case EFF_EX_RETR_NOTE:
                                    tmp = NOTE_EFF_D_LO(current_line[c_note].data);
                                    if (tick % tmp == 0) {
                                        chn_seek(c_note, 2);
                                    }
                                    break;
                                case EFF_EX_F_VOL_UP:
                                    if (tick == 0) {
                                        chn_vol_add(c_note, NOTE_EFF_D_LO(current_line[c_note].data));
                                    }
                                    break;
                                case EFF_EX_F_VOL_DOWN:
                                    if (tick == 0) {
                                        chn_vol_add(c_note, -NOTE_EFF_D_LO(current_line[c_note].data));
                                    }
                                    break;
                                case EFF_EX_NOTE_CUT:
                                    if (tick == NOTE_EFF_D_LO(current_line[c_note].data)) {
                                        chn_set_vol(c_note, 0);
                                    }
                                    break;
                                case EFF_EX_NOTE_DEL:
                                    // TODO
                                    break;
                                case EFF_EX_PATT_DEL:
                                    if (tick == 0) {
                                        delay_fac = NOTE_EFF_D_LO(current_line[c_note].data);
                                    }
                                    break;
                                case EFF_EX_INV_LOOP:
                                    // TODO no attempt to support that ugly effect
                                    break;
                            } // end switch extended
                            break;
                        case EFF_TEMPO:
                            if (tick == 0) {
                                tmp = NOTE_EFF_DATA(current_line[c_note].data);
                                if (tmp == 0) {
                                    stop_next = true;
                                } else if (tmp > 0 && tmp < 32) {
                                    speed = tmp;
                                } else {
                                    speed = 6;
                                    //fprintf(stderr, "Invalid Speed Change\n");
                                    //stop_next = true;
                                }
                            }
                            break;
                        default:
                            fprintf(stderr, 
                                    "Invalid effect number: %d\n", 
                                    NOTE_EFF_CMD(current_line[c_note].data));
                            break;
                    }

                }
                // write data to PortAudio and wait until data has benn processed
                mixer_render();
                (void)total_ticks;
                //printf("written %llu buffers to audio device\n", total_ticks++);
            } // end of tick loop
            if (line_break) goto line_start;
            if (stop_next) goto stop_song;
        } // end of line loop
    } // end of pattern loop
stop_song:

    stop_mixer();

    return true;
}


