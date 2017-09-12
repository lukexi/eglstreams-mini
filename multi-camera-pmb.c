/*
This runs the camera in a thread and passes buffers over to the main thread
for copying into a persistent mapped buffer.
The memcpy takes about 1ms, but this otherwise runs well.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <GL/glew.h>
#include <math.h>
#include <pthread.h>

#include "egl.h"
#include "utils.h"

#include "camera.h"
#include "shader.h"
#include "quad.h"
#include "texture.h"
#include "mvar.h"
#include "texture-buffer.h"
#include "global-state.h"

typedef struct {
    mvar*           ImageMVar;
    texture_buffer* TexBuf;
    char*           DeviceName;
} camera_info;


#define NUM_CAMERAS 3
camera_info CameraInfos[NUM_CAMERAS];

void* CameraThreadMain(void* Args) {
    camera_info* Info = (camera_info*)Args;

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open(
        Info->DeviceName,
        Info->DeviceName,
        CameraWidth,
        CameraHeight,
        CameraFPS,
        IsAsync);

    if (CameraState == NULL) Fatal("Couldn't find a camera : (\n");

    camera_set_ctrl_by_name(CameraState, "exposure_auto", 1);
    int ExposureMin, ExposureMax;
    camera_get_ctrl_bounds_by_name(CameraState, "exposure_absolute", &ExposureMin, &ExposureMax);
    int ExposureScale = ExposureMax - ExposureMin;


    printf("Min: %i Max: %i Scale: %i\n", ExposureMin, ExposureMax, ExposureScale);
    while (1) {

        float Y = (sin(GetTime()) * 0.5 + 0.5);
        Y = 0.1 + Y*0.1;
        camera_set_ctrl_by_name(CameraState, "exposure_absolute", ExposureMin + Y * ExposureScale);
        // printf("Setting to %f\n", ExposureMin + Y * ExposureScale);

        uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);
        camera_capture(CameraState, CameraBuffer);
        TryWriteMVar(Info->ImageMVar, CameraBuffer);
    }

    return NULL;
}

void CreateCamera(camera_info* Info, char* DeviceName) {

    Info->TexBuf = CreateBufferedTexture(CameraWidth, CameraHeight, CameraChannels);
    Info->ImageMVar = CreateMVar(free);
    Info->DeviceName = DeviceName;

    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, Info);
    assert(!ResultCode);
}


int main() {
    GetTime();

    egl_state* EGL = SetupEGLThreaded();

    egl_display* Display = &EGL->Displays[0];
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    eglSwapInterval(Display->DisplayDevice, 0);
    glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured-trans.frag"
        );
    GLuint FullscreenQuadVAO = CreateFullscreenQuad();

    CreateCamera(&CameraInfos[0], "/dev/video0");
    CreateCamera(&CameraInfos[1], "/dev/video1");
    CreateCamera(&CameraInfos[2], "/dev/video2");

    // glClearColor(1, 1, 1, 1);
    glClearColor(
                (sin(GetTime()*3)/2+0.5) * 0.8,
                (sin(GetTime()*5)/2+0.5) * 0.8,
                (sin(GetTime()*7)/2+0.5) * 0.8,
                1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(FullscreenQuadProgram);
    glBindVertexArray(FullscreenQuadVAO);

    while (1) {

        for (int CameraIndex = 0; CameraIndex < NUM_CAMERAS; CameraIndex++) {

            camera_info* Info = &CameraInfos[CameraIndex];

            // Update camera
            uint8_t* NewCameraBuffer = (uint8_t*)TryReadMVar(Info->ImageMVar);
            if (NewCameraBuffer) {
                UploadToBufferedTexture(Info->TexBuf, NewCameraBuffer);
                free(NewCameraBuffer);
            }

            // Draw
            glBindTexture(GL_TEXTURE_2D, GetReadableTexture(Info->TexBuf));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        // NEWTIME(Swap);
        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        // GRAPHTIME(Swap, "+");
        GLCheck("Display Thread");
    }

    return 0;
}
