#ifndef DOTDETECTOR_H
#define DOTDETECTOR_H

#include "dotframe.h"

#define MAX_DOTS 16384

typedef struct {
    GLuint Program;
    GLuint SSBO;
    GLuint Query;
} dotdetector_state;

dotdetector_state* InitializeDotDetector ();

int DetectDots (dotdetector_state* Detector,
                int MinRadius, int MaxRadius,
                GLuint CameraTexID, GLenum CameraTexFormat,
                dot_t* OutDots);

void CleanupDotDetector (dotdetector_state* Detector);

#endif // DOTDETECTOR_H
