#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_framing.h>

#include "libs/portaudio.h"

#define NUM_CHANNELS (2)
#define NUM_SECONDS (0.1)
#define SAMPLE_RATE (8000)
#define NUM_BINS (32)
#define SMOOTH_FACTOR (0.8)
/* Select sample format. */
#if 0
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
    fftw_complex *recordedSamples;
    fftw_complex *fftwOutput;
}   
paTestData;

//Plan global
fftw_plan p;

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    fftw_complex *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
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
        finished = paContinue;
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
            *wptr[0] = SAMPLE_SILENCE;  /* left */
            wptr++;
            if( NUM_CHANNELS == 2 ) {
              *wptr[0] = SAMPLE_SILENCE;
              wptr++;
            }
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr[0] = *rptr++;  /* left */
            wptr++;
            if( NUM_CHANNELS == 2 ) {
              *wptr[0] = *rptr++;
              wptr++;
            }
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}


int main(int argc, char* argv[]) {


    printf("Starting connection to message broker...\n");
    amqp_socket_t *socket = NULL;
    amqp_connection_state_t conn;
    int status;

    conn = amqp_new_connection();

    socket = amqp_tcp_socket_new(conn);
    if(!socket) {
        printf("Failed creating SSL/TLS socket\n");
        return 1;
    }

    status = amqp_socket_open(socket, "localhost", 5672);

    if(status) {
        printf("Failed opening SSL/TLS connection\n");
        return 1;
    }

    amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    amqp_channel_open(conn, 1);
    amqp_get_rpc_reply(conn);
    

    printf("Initialising PortAudio...\n");
    PaError err;
    
    err = Pa_Initialize();

    if( err != paNoError) {
        error(err);
        return 1;
    }

    printf( "PortAudio version: 0x%08X\n", Pa_GetVersion());

    PaStreamParameters  inputParameters;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;

    printf("patest_record.c\n"); fflush(stdout);

    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(fftw_complex);
    data.recordedSamples = (fftw_complex *) fftw_malloc( numBytes ); /* From now on, recordedSamples is initialised. */
    data.fftwOutput = (fftw_complex *) fftw_malloc( numBytes );
    if( data.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        error(err);
        return 1;
    }
    for( i=0; i<numSamples; i++ ) data.recordedSamples[i][0] = 0;
    for( i=0; i<numSamples; i++ ) data.recordedSamples[i][1] = 0;

    //Before we begin gathering sound data, create an fftw plan
    printf("Generating fft plan. May take some time...\n");
    p = fftw_plan_dft_1d(numSamples, data.recordedSamples, data.fftwOutput, FFTW_FORWARD, FFTW_MEASURE);
    printf("Plan generated.\n");


    inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;


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

    double sum[NUM_BINS];

    printf("Reading audio and sending results to message broker...\n");
    while(1){
        data.frameIndex = 0;
        for(i=0;i<NUM_BINS;i++){
            sum[i] = 0;
        }
        Pa_Sleep(1000*NUM_SECONDS);
        fftw_execute(p);
        for(i=1;i<totalFrames; i++) {
            //downsample to amount of LEDs
            int index = (int) floor(i*(float)NUM_BINS/(float)totalFrames);
            sum[index] += data.fftwOutput[i][0];
            sum[index] += data.fftwOutput[i][1];
        }

        //Add some white noise to drown out background noises
        for(i=0; i<NUM_BINS; i++){
            if(sum[i]) {
              sum[i] += 50;
            }
        }

        //Stretch the results to better fit human hearing
        for(i=0; i<NUM_BINS; i++) {
            sum[i] = 10 * log10((sum[i] * sum[i]));
            //Case for where sum is zero, log returns -inf
            if(isinf(sum[i])) {
              sum[i] = 0.0;
            }
        }

        //Smooth the results
        for(i=1; i<NUM_BINS; i++) {
          sum[i] = SMOOTH_FACTOR * sum[i] + (1 - SMOOTH_FACTOR) * sum[i-1];
        }

        //Give enough space for all numbers plus commas
        char* messagebody = (char*) malloc(16*sizeof(char)*(NUM_BINS));
        memset(messagebody, 0, 16*NUM_BINS);
        char currentbin[16];
        memset(currentbin, 0, 16);
        //Build message by concatenating strings
        for(int i=0;i<NUM_BINS-1;i++){
            snprintf(currentbin, 16, "%f,", sum[i]);
            messagebody = strcat(messagebody, currentbin);
        }
        snprintf(currentbin, 16, "%f", sum[NUM_BINS-1]);
        messagebody = strcat(messagebody, currentbin);

        amqp_basic_properties_t props;
        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
        props.content_type = amqp_cstring_bytes("text/plain");
        props.delivery_mode = 2; /* persistent delivery mode */

        amqp_basic_publish(conn,
            1,
            amqp_cstring_bytes(""),
            amqp_cstring_bytes("primary-queue"),
            0,
            0,
            &props,
            amqp_cstring_bytes(messagebody));
    }

    //Terminate amqp connection
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);

    //End portaudio bindings

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

    //Once stopped and closed, destroy plan
    fftw_destroy_plan(p);


    err = Pa_Terminate();
    if( err != paNoError) {
        error(err);
        return 1;
    }

    return 0;
}
