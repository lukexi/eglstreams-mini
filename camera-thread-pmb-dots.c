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
#include "dotdetector.h"
#include "dotframe.h"
#include "texture.h"
#include "texture-buffer.h"
#include "mvar.h"
#include "global-state.h"



#define MAX_FRAMES 600

void* CameraThreadMain(void* Args) {
    mvar* CameraMVar = (mvar*)Args;

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    fps FPS = MakeFPS("Capture Rate");
    while (1) {
        uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);
        camera_capture(CameraState, CameraBuffer);
        TryWriteMVar(CameraMVar, CameraBuffer);
        TickFPS(&FPS);
    }

    return NULL;
}


int main() {
    GetTime();

    egl_state* EGL = SetupEGLThreaded();

    EnableGLDebug();

    egl_display* Display = &EGL->Displays[0];
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    eglSwapInterval(Display->DisplayDevice, 1);
    glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );
    GLuint FullscreenQuadVAO = CreateFullscreenQuad();


    texture_buffer* CameraBufTex = CreateBufferedTexture(CameraWidth, CameraHeight, CameraChannels);

    // Camera setup
    mvar* CameraMVar = CreateMVar(free);
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraMVar);
    assert(!ResultCode);

    dotdetector_state* DotDetector = InitializeDotDetector();
    dot_t* Dots = malloc(MAX_DOTS * sizeof(dot_t));

    frame_t* Frames = NULL;
    int FramesCount = 0;

    fps FPS = MakeFPS("Display Rate");
    fps CamFPS = MakeFPS("Camera Rate");
    while (1) {

        // Update camera
        uint8_t* NewCameraBuffer = (uint8_t*)TryReadMVar(CameraMVar);
        if (NewCameraBuffer) {
            NEWTIME(UpdateTexture);

            UploadToBufferedTexture(CameraBufTex, NewCameraBuffer);
            free(NewCameraBuffer);

            GRAPHTIME(UpdateTexture, "*");

#if 1
            NEWTIME(DetectDots);
            int DotsCount = DetectDots(DotDetector,
                    4, 12,
                    GetReadableTexture(CameraBufTex), GL_RGBA8,
                    Dots);
            GRAPHTIME(DetectDots, ".");
            printf("Got dots: %i\n", DotsCount);

            NEWTIME(Frames);
            frame_t* NewCameraFrames = malloc(MAX_FRAMES * sizeof(frame_t));
            int NewCameraFramesCount = dotframe_update_frames(
                Dots, DotsCount,
                Frames, FramesCount,
                NewCameraFrames, MAX_FRAMES - FramesCount);
            if (Frames) free(Frames);
            Frames      = NewCameraFrames;
            FramesCount = NewCameraFramesCount;
            GRAPHTIME(Frames, "]");
#endif
            TickFPS(&CamFPS);
        }

        // printf(">%i %i>\n", BufferIndex, DrawIndex);

        // Draw

        NEWTIME(Draw);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, GetReadableTexture(CameraBufTex));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        GRAPHTIME(Draw, "/");

        // NEWTIME(Swap);
        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        // GRAPHTIME(Swap, "+");
        GLCheck("Display Thread");

        QueryDotDetectorComputeTime(DotDetector);

        TickFPS(&FPS);
    }

    return 0;
}
