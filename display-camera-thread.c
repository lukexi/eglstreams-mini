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
        glFlush();
        GLCheck("Camera Thread");
    }

    return NULL;
}

void* DisplayThreadMain(void* ThreadArguments) {
    egl_display* Display = (egl_display*)ThreadArguments;
    char* DisplayName = Display->EDID->MonitorName;
    printf("Starting display %s\n", DisplayName);
    // Set the display's framebuffer as active
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

    GLuint FullscreenQuadVAO = CreateFullscreenQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    while (1) {
        printf("Drawing %s\n", DisplayName);
        // Draw a texture to the display framebuffer
        glClearColor(
            1,
            1,
            1,
            1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, CameraTexID);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        char Pixel[3];
        glReadPixels(0,0,1,1,GL_RGB,GL_UNSIGNED_BYTE,&Pixel);
        printf("Pixel %s: %i %i %i\n", DisplayName, Pixel[0],Pixel[1],Pixel[2]);

        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        usleep(16600);
        GLCheck("Display Thread");
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
    EGLContext RootContext      = GetEglContext(eglDisplayDevice, eglConfig);
    EGLBoolean ret = eglMakeCurrent(eglDisplayDevice, EGL_NO_SURFACE, EGL_NO_SURFACE, RootContext);
    assert(ret);
    InitGLEW();



    // Set up EGL state for each connected display
    int NumDisplays;
    kms_plane* Planes     = SetDisplayModes(drmFd, &NumDisplays);

    // Displays share root context
    egl_display* Displays = GetEglDisplays(eglDisplayDevice, eglConfig, RootContext, Planes, NumDisplays);

    // NumDisplays = 1; printf("Forcing number of displays to 1\n");

    // Camera shares root context
    EGLint contextAttribs[] = { EGL_NONE };
    EGLContext CameraThreadContext =
        eglCreateContext(
            eglDisplayDevice,
            eglConfig,
            RootContext,
            contextAttribs);

    // Set up global resources
    CameraTexID = CreateTexture(CameraWidth, CameraHeight, CameraChannels);
    FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );

    // Launch camera thread
    camera_thread_state* CameraThreadState = malloc(sizeof(camera_thread_state));
    CameraThreadState->MVar                = CreateMVar(free);
    CameraThreadState->Context             = CameraThreadContext;
    CameraThreadState->DisplayDevice       = eglDisplayDevice;
    CameraThreadState->TexID               = CameraTexID;
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraThreadState);
    assert(!ResultCode);


    // Launch display threads
    for (int D = 0; D < NumDisplays; D++) {
        egl_display* Display = &Displays[D];
        pthread_t DisplayThread;
        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, Display);
        assert(!ResultCode);
    }
    GLCheck("Hello");

    pthread_join(CameraThread, NULL);

    return 0;
}