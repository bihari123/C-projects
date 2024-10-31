#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "portaudio.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PACKET_SIZE 65507

#define PORT 8080
#define SERVER_IP "127.0.0.1"

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS (10)
#define NUM_CHANNELS (1)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG (0) /**/
/** Set to 1 if you want to capture the recording to a file. */
#define WRITE_TO_FILE (0)

/* Select sample format. */
#if 1
#define PA_SAMPLE_TYPE paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE (128)
#define PRINTF_S_FORMAT "%d"
#endif

#define EOT_CODE -9999.0f // Define an end-of-transmission code

typedef struct {
  int frameIndex; /* Index into sample array. */
  int maxFrameIndex;
  SAMPLE *recordedSamples;
} paTestData;
/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int playCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags, void *userData) {
  paTestData *data = (paTestData *)userData;
  SAMPLE *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
  SAMPLE *wptr = (SAMPLE *)outputBuffer;
  unsigned int i;
  int finished;
  unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;

  (void)inputBuffer; /* Prevent unused variable warnings. */
  (void)timeInfo;
  (void)statusFlags;
  (void)userData;

  if (framesLeft < framesPerBuffer) {
    /* final buffer... */
    for (i = 0; i < framesLeft; i++) {
      *wptr++ = *rptr++; /* left */
      if (NUM_CHANNELS == 2)
        *wptr++ = *rptr++; /* right */
    }
    for (; i < framesPerBuffer; i++) {
      *wptr++ = 0; /* left */
      if (NUM_CHANNELS == 2)
        *wptr++ = 0; /* right */
    }
    data->frameIndex += framesLeft;
    finished = paComplete;
  } else {
    for (i = 0; i < framesPerBuffer; i++) {
      *wptr++ = *rptr++; /* left */
      if (NUM_CHANNELS == 2)
        *wptr++ = *rptr++; /* right */
    }
    data->frameIndex += framesPerBuffer;
    finished = paContinue;
  }
  return finished;
}
int main() {

  PaStreamParameters inputParameters, outputParameters;
  PaStream *stream;
  PaError err = paNoError;
  paTestData data;
  int i;
  int totalFrames;
  int numSamples;
  int numBytes;
  SAMPLE max, val;
  double average;
  float sampleArr[440999];
  int packet_size;
  int num_floats;
  socklen_t addr_len;
  int received_bytes;
  int total_received = 0;
  fflush(stdout);

  data.maxFrameIndex = totalFrames =
      NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
  data.frameIndex = 0;
  numSamples = totalFrames * NUM_CHANNELS;

  numBytes = numSamples * sizeof(SAMPLE);
  data.recordedSamples = (SAMPLE *)malloc(
      numBytes); /* From now on, recordedSamples is initialised. */
  if (data.recordedSamples == NULL) {
    printf("Could not allocate record array.\n");
    goto done;
  }
  for (i = 0; i < numSamples; i++)
    data.recordedSamples[i] = 0;

  err = Pa_Initialize();
  if (err != paNoError)
    goto done;

  inputParameters.device =
      Pa_GetDefaultInputDevice(); /* default input device */
  if (inputParameters.device == paNoDevice) {
    fprintf(stderr, "Error: No default input device.\n");
    goto done;
  }
  inputParameters.channelCount = NUM_CHANNELS;
  inputParameters.sampleFormat = PA_SAMPLE_TYPE;
  inputParameters.suggestedLatency =
      Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  int sockfd;
  float buffer[BUFFER_SIZE];
  struct sockaddr_in serverAddr, clientAddr;
  socklen_t clientAddrLen = sizeof(clientAddr);

  // Creating socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Filling server information
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(PORT);

  // Binding socket
  if (bind(sockfd, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
      0) {
    perror("Bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", PORT);
  // Calculate the number of floats we can receive in each packet
  packet_size = MAX_PACKET_SIZE;
  num_floats = packet_size / sizeof(float);

  while (1) {
    while (total_received < 440999) {
      float buffer[num_floats];
      int floats_to_receive = (440999 - total_received) < num_floats
                                  ? (440999 - total_received)
                                  : num_floats;

      received_bytes =
          recvfrom(sockfd, buffer, floats_to_receive * sizeof(float), 0,
                   (struct sockaddr *)&clientAddr, &addr_len);
      if (received_bytes < 0) {
        perror("Receive failed");
        close(sockfd);
        exit(EXIT_FAILURE);
      }
      // Check for the EOT code
      if (received_bytes == sizeof(EOT_CODE) && buffer[0] == EOT_CODE) {
        break;
      }
      // Copy the received data to the main array
      memcpy(&sampleArr[total_received], buffer, received_bytes);
      total_received += received_bytes / sizeof(float);
    }
    // Receive message from client
    // int n = recvfrom(sockfd, sampleArr, 1024, MSG_WAITALL,
    // (struct sockaddr *)&clientAddr, &clientAddrLen);
    // if (n < 0) {
    // perror("Receive failed");
    // close(sockfd);
    // exit(EXIT_FAILURE);
    // }

    for (i = 0; i < numSamples; i++) {
      printf("%d th is %f\n", i, sampleArr[i]);
      data.recordedSamples[i] = sampleArr[i];
    }
    data.frameIndex = 0;

    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr, "Error: No default output device.\n");
      goto done;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency =
        Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    printf("\n=== Now playing back. ===\n");
    fflush(stdout);
    err = Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE,
                        FRAMES_PER_BUFFER, paClipOff, playCallback, &data);
    if (err != paNoError)
      goto done;

    if (stream) {
      err = Pa_StartStream(stream);
      if (err != paNoError)
        goto done;

      printf("Waiting for playback to finish.\n");
      fflush(stdout);

      while ((err = Pa_IsStreamActive(stream)) == 1)
        Pa_Sleep(100);
      if (err < 0)
        goto done;

      err = Pa_CloseStream(stream);
      if (err != paNoError)
        goto done;

      printf("Done.\n");
      fflush(stdout);

      // Send confirmation message to client
      const char *message = "Message received";
      if (sendto(sockfd, message, strlen(message), MSG_CONFIRM,
                 (const struct sockaddr *)&clientAddr, clientAddrLen) < 0) {
        perror("Send failed");
        close(sockfd);
        exit(EXIT_FAILURE);
      }
      printf("Confirmation message sent to client\n");
    }

  done:
    fprintf(stdout, "inside done\n");
    Pa_Terminate();
    if (data.recordedSamples) { /* Sure it is NULL or valid. */
      fprintf(stdout, "freeing the recorded samples\n");
      free(data.recordedSamples);

      data.maxFrameIndex = totalFrames =
          NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
      data.frameIndex = 0;
      numSamples = totalFrames * NUM_CHANNELS;
      numBytes = numSamples * sizeof(SAMPLE);
      data.recordedSamples = (SAMPLE *)malloc(
          numBytes); /* From now on, recordedSamples is initialised. */
      if (data.recordedSamples == NULL) {
        printf("Could not allocate record array.\n");
      }
      for (i = 0; i < numSamples; i++)
        data.recordedSamples[i] = 0;

      // if (err != paNoError)
      // goto done;
    }
    if (err != paNoError) {
      fprintf(stderr, "An error occurred while using the portaudio stream\n");
      fprintf(stderr, "Error number: %d\n", err);
      fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
      fflush(stdout);
      err = 1; /* Always return 0 or 1, but no other return codes. */
    }
    close(sockfd);

    return -1;

    // Close the socket
    close(sockfd);
    return 0;
  }
}
