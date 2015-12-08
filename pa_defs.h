#pragma once

#include "portaudio.h"

typedef int PaStreamCallback( const void *input, void *output, unsigned long frameCount, 
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
        void *userData ) ;

typedef struct {
    float left_phase;
    float right_phase;
} paTestData;
