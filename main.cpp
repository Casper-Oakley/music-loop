#include <stdio.h>
#include <stdlib.h>
#include "libs/portaudio.h"

#define NUM_CHANNELS (2)
#define NUM_SECONDS (2)
#define SAMPLE_RATE (44100)
/* Select sample format. */
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 0
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif


void error(PaError err) {
    Pa_Terminate();
    fprintf( stderr, "Error number: %d\n", err);
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText(err));
}

typedef struct
{
    int frameIndex;
    int maxFrameIndex;
    SAMPLE *recordedSamples;
}   
paTestData;

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;
    
    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;
    
    if( framesLeft < framesPerBuffer )
    {
        framesToCalc = framesLeft;
        finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }
    
    if( inputBuffer == NULL )
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    printf("%d\n", data->frameIndex);
    return finished;
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

    PaStreamParameters  inputParameters,
    outputParameters;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;

    printf("patest_record.c\n"); fflush(stdout);

    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *) malloc( numBytes ); /* From now on, recordedSamples is initialised. */
    if( data.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        error(err);
        return 1;
    }
    for( i=0; i<numSamples; i++ ) data.recordedSamples[i] = 0;

    inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    int numDevices = Pa_GetDeviceCount();

    const   PaDeviceInfo *deviceInfo;
    for(int i=0; i<numDevices; i++ )
    {
        deviceInfo = Pa_GetDeviceInfo( i );
        printf("%s\n", deviceInfo->name);
        printf("%d\n", deviceInfo->maxOutputChannels);
    }

    PaStream *stream;
    /* Open an audio I/O stream. */
    err = Pa_OpenStream( &stream,
                                &inputParameters,
                                NULL,
                                SAMPLE_RATE,
                                256,
                                paClipOff,
                                patestCallback,
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

    Pa_Sleep(1000*NUM_SECONDS);

    double sum = 0;

    for(int i=0;i<totalFrames; i++) {
        sum += data.recordedSamples[i];
    }

    printf("%f\n", sum);

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
