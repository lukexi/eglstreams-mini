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


int main() {

    // Setup global EGL device state
    GetEglExtensionFunctionPointers();

    EGLDeviceEXT eglDevice = GetEglDevice();

    int drmFd = GetDrmFd(eglDevice);

    EGLDisplay eglDpy      = GetEglDisplay(eglDevice, drmFd);
    EGLConfig  eglConfig   = GetEglConfig(eglDpy);
    EGLContext RootContext = GetEglContext(eglDpy, eglConfig);

    EGLBoolean ret = eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, RootContext);
    if (!ret) Fatal("Couldn't make main context current\n");

    InitGLEW();


    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/shaded.frag"
        );


    // Set up EGL state for each connected display
    int NumDisplays;
    kms_plane* Planes     = SetDisplayModes(drmFd, &NumDisplays);
    egl_display* Displays = SetupEGLDisplays(eglDpy, eglConfig, RootContext, Planes, NumDisplays);

    // Create a VAO for each display, since those aren't shared
    // (c.f. "Container objects")
    GLuint* VAOs = malloc(sizeof(GLuint) * NumDisplays);
    for (int D = 0; D < NumDisplays; D++) {
        egl_display* Display = &Displays[D];
        // Set the display's framebuffer as active
        eglMakeCurrent(Display->DisplayDevice,
            Display->Surface, Display->Surface,
            Display->Context);
        GLuint FullscreenQuadVAO = CreateFullscreenQuad();
        VAOs[D] = FullscreenQuadVAO;
    }


    while (1) {



        for (int D = 0; D < NumDisplays; D++) {
            egl_display* Display = &Displays[D];
            // Set the display's framebuffer as active
            eglMakeCurrent(Display->DisplayDevice,
                Display->Surface, Display->Surface,
                Display->Context);
            glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

            // Draw a shader to the display framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(
                (sin(GetTime()*3)/2+0.5) * 0.8,
                (sin(GetTime()*5)/2+0.5) * 0.8,
                (sin(GetTime()*7)/2+0.5) * 0.8,
                1);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(FullscreenQuadProgram);
            glBindVertexArray(VAOs[D]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            eglSwapBuffers(
                Display->DisplayDevice,
                Display->Surface);
        }
    }

    return 0;
}
