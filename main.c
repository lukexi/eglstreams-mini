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

static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;

GLuint FullscreenQuadProgram;
GLuint CameraTexID;

typedef struct {
    EGLContext Context;
    mvar* MVar;
    GLuint TexID;
    EGLDisplay DisplayDevice;
} camera_thread_state;

void* CameraThreadMain(void* Args) {
    camera_thread_state* CameraThreadState = (camera_thread_state*)Args;

    EGLBoolean ret = eglMakeCurrent(
        CameraThreadState->DisplayDevice,
        EGL_NO_SURFACE, EGL_NO_SURFACE,
        CameraThreadState->Context);
    if (!ret) {
        // EGLCheck("CameraThreadMain");
        Fatal("Couldn't make thread context current\n");
    }

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }
    uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);

    while (1) {
        camera_capture(CameraState, CameraBuffer);
        UpdateTexture(CameraThreadState->TexID, CameraWidth, CameraHeight, GL_RGB, CameraBuffer);
        glFinish();
    }

    return NULL;
}

/*
Root
    Camera
    Display1
    Display2
Doesn't work.

Root
    Display1
        Camera
    Display2
Works

*/

void* DisplayThreadMain(void* ThreadArguments) {
    printf("Starting display %p\n", ThreadArguments);
    egl_display* Display = (egl_display*)ThreadArguments;
    // Set the display's framebuffer as active
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

    GLuint FullscreenQuadVAO = CreateFullscreenQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    while (1) {
        printf("Drawing %p\n", ThreadArguments);
        // Draw a texture to the display framebuffer
        glClearColor(
            (sin(GetTime()*3)/2+0.5) * 0.8,
            (sin(GetTime()*5)/2+0.5) * 0.8,
            (sin(GetTime()*7)/2+0.5) * 0.8,
            1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, CameraTexID);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        usleep(10000);
    }
}

int main() {
    GetTime();
    // EGL setup
    // Setup global EGL device state
    GetEglExtensionFunctionPointers();

    EGLDeviceEXT eglDevice = GetEglDevice();

    int drmFd = GetDrmFd(eglDevice);

    EGLDisplay eglDisplayDevice = GetEglDisplay(eglDevice, drmFd);
    EGLConfig  eglConfig        = GetEglConfig(eglDisplayDevice);
    EGLContext eglContext       = GetEglContext(eglDisplayDevice, eglConfig);
    EGLBoolean ret = eglMakeCurrent(
        eglDisplayDevice,
        EGL_NO_SURFACE, EGL_NO_SURFACE,
        eglContext);
    InitGLEW();

    // Set up EGL state for each connected display
    int NumDisplays;
    kms_plane* Planes     = SetDisplayModes(drmFd, &NumDisplays);

    egl_display* Displays = GetEglDisplays(eglDisplayDevice, eglConfig, eglContext, Planes, NumDisplays);

    EGLint contextAttribs[] = { EGL_NONE };
    EGLContext CameraThreadContext =
        eglCreateContext(
            eglDisplayDevice,
            eglConfig,
            eglContext,
            contextAttribs);

    CameraTexID = CreateTexture(CameraWidth, CameraHeight, CameraChannels);


    camera_thread_state* CameraThreadState = malloc(sizeof(camera_thread_state));
    CameraThreadState->MVar                = CreateMVar(free);
    CameraThreadState->Context             = CameraThreadContext;
    CameraThreadState->DisplayDevice       = eglDisplayDevice;
    CameraThreadState->TexID               = CameraTexID;
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraThreadState);
    assert(!ResultCode);

    FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );

    for (int D = 0; D < NumDisplays; D++) {
        egl_display* Display = &Displays[D];
        pthread_t DisplayThread;
        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, Display);
        assert(!ResultCode);
    }

    pthread_join(&CameraThread, NULL);

    return 0;
}
