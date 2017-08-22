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

#include "shader.h"
#include "quad.h"
#include "texture.h"
#include "mvar.h"

GLuint FullscreenQuadProgram;

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
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
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
    if (!ret) Fatal("Couldn't make main context current\n");

    InitGLEW();


    // Set up global resources
    FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/shaded.frag"
        );


    // Set up EGL state for each connected display
    int NumDisplays;
    kms_plane* Planes     = SetDisplayModes(drmFd, &NumDisplays);
    egl_display* Displays = GetEglDisplays(eglDisplayDevice, eglConfig, RootContext, Planes, NumDisplays);


    // Launch display threads
    pthread_t DisplayThread; // Hold onto the last thread so we can join on it
    for (int D = 0; D < NumDisplays; D++) {
        egl_display* Display = &Displays[D];
        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, Display);
        assert(!ResultCode);
    }
    GLCheck("Hello");

    pthread_join(DisplayThread, NULL);

    return 0;
}
