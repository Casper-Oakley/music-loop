#include <stdio.h>
#include "libs/portaudio.h"


void error(PaError err) {
    Pa_Terminate();
    fprintf( stderr, "Error number: %d\n", err);
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText(err));
}

typedef struct
{
    float left_phase;
    float right_phase;
}   
paTestData;

static paTestData data;

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    /* Cast data passed through stream to our structure. */
    paTestData *data = (paTestData*)userData; 
    float *out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; /* Prevent unused variable warning. */
    
    for( i=0; i<framesPerBuffer; i++ )
    {
        *out++ = data->left_phase;  /* left */
        *out++ = data->right_phase;  /* right */
        /* Generate simple sawtooth phaser that ranges between -1.0 and 1.0. */
        data->left_phase += 0.01f;
        /* When signal reaches top, drop back down. */
        if( data->left_phase >= 1.0f ) data->left_phase -= 2.0f;
        /* higher pitch so we can distinguish left and right. */
        data->right_phase += 0.03f;
        if( data->right_phase >= 1.0f ) data->right_phase -= 2.0f;
    }
    return 0;
}


int main(int argc, char* argv[]) {
    printf("Hello, World!");
    PaError err;
    
    err = Pa_Initialize();

    if( err != paNoError) {
        error(err);
        return 1;
    }

    printf( "PortAudio version: 0x%08X\n", Pa_GetVersion());

    PaStream *stream;
    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                0,          /* no input channels */
                                2,          /* stereo output */
                                paFloat32,  /* 32 bit floating point output */
                                44100,
                                256,        /* frames per buffer, i.e. the number
                                                   of sample frames that PortAudio will
                                                   request from the callback. Many apps
                                                   may want to use
                                                   paFramesPerBufferUnspecified, which
                                                   tells PortAudio to pick the best,
                                                   possibly changing, buffer size.*/
                                patestCallback, /* this is your callback function */
                                &data ); /*This is a pointer that will be passed to
                                                   your callback*/
    if( err != paNoError) {
        error(err);
        return 1;
    }

    err = Pa_StartStream( stream );
    if( err != paNoError) {
        error(err);
        return 1;
    }

    Pa_Sleep(10000);

    err = Pa_StopStream( stream );
    if( err != paNoError) {
        error(err);
        return 1;
    }

    err = Pa_CloseStream( stream );
    if( err != paNoError) {
        error(err);
        return 1;
    }

    err = Pa_Terminate();
    if( err != paNoError) {
        error(err);
        return 1;
    }

    return 0;
}
