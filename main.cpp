#include <stdio.h>
#include "libs/portaudio.h"


void error(PaError err) {
    Pa_Terminate();
    fprintf( stderr, "Error number: %d\n", err);
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText(err));
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
    return 0;
}
