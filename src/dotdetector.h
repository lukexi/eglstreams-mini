#ifndef DOTDETECTOR_H
#define DOTDETECTOR_H

#include "dotframe.h"
#include "buffered-texture.h"

#define MAX_DOTS 16384

typedef struct {
    float x;
    float y;
    float rx;
    float ry;
    float L;
    float a;
    float b;
} shader_dot;

typedef struct {
    shader_dot Dots[MAX_DOTS];
    int NumDots;
} found_dots;

typedef struct {
    GLuint Program;
    GLuint SSBO;
    GLuint Query;
    void* SSBOMemory;
    GLsync Syncs[PMB_SIZE];
    int WriteIndex;
    int ReadIndex;
    size_t ElementSize;
    size_t AlignedElementSize;
} dotdetector_state;

dotdetector_state* InitializeDotDetector ();

int DetectDots (dotdetector_state* Detector,
                int MinRadius, int MaxRadius,
                GLuint CameraTexID, GLenum CameraTexFormat,
                dot_t* OutDots);

void QueryDotDetectorComputeTime(dotdetector_state* Detector);

void CleanupDotDetector (dotdetector_state* Detector);

#endif // DOTDETECTOR_H
