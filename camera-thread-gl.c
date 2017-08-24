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

typedef struct {
    EGLContext Context;
    GLuint TexID;
    EGLDisplay DisplayDevice;
} camera_thread_state;

#define NEWTIME(name) float __name##Before = GetTime();
#define ENDTIME(name) printf("Took: %.2fms\n", (GetTime() - __name##Before) * 1000);
#define GRAPHTIME(name) Graph((GetTime() - __name##Before) * 1000);

void Graph(int N) {
    for (int i = 0; i < N; ++i) {
        printf("*");
    }
    printf("\n");
}

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

        NEWTIME(UpdateTexture);
        glTextureSubImage2D(
            CameraThreadState->TexID,
            0,
            0, 0,
            CameraWidth, CameraHeight,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            CameraBuffer);
        glFlush();
        GRAPHTIME(UpdateTexture);
    }

    return NULL;
}



int main() {
    GetTime();

    egl_state* EGL = SetupEGL();

    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );

    GLuint* VAOs = malloc(sizeof(GLuint) * EGL->DisplaysCount);
    for (int D = 0; D < EGL->DisplaysCount; D++) {
        egl_display* Display = &EGL->Displays[D];
        // Set the display's framebuffer as active
        eglMakeCurrent(
            Display->DisplayDevice,
            Display->Surface, Display->Surface,
            Display->Context);
        GLuint FullscreenQuadVAO = CreateFullscreenQuad();
        VAOs[D] = FullscreenQuadVAO;
    }

    GLuint CameraTexID = CreateTexture(CameraWidth, CameraHeight, CameraChannels);

    // Launch camera thread
    EGLint ContextAttribs[] = { EGL_NONE };
    EGLContext CameraThreadContext = eglCreateContext(
        EGL->DisplayDevice,
        EGL->Config,
        EGL->RootContext,
        ContextAttribs);
    camera_thread_state* CameraThreadState = malloc(sizeof(camera_thread_state));
    CameraThreadState->Context             = CameraThreadContext;
    CameraThreadState->DisplayDevice       = EGL->DisplayDevice;
    CameraThreadState->TexID               = CameraTexID;

    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraThreadState);
    assert(!ResultCode);

    while (1) {

        for (int D = 0; D < EGL->DisplaysCount; D++) {
            egl_display* Display = &EGL->Displays[D];
            // Set the display's framebuffer as active
            eglMakeCurrent(
                Display->DisplayDevice,
                Display->Surface, Display->Surface,
                Display->Context);
            glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

            // Draw a texture to the display framebuffer
            // glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(
                (sin(GetTime()*3)/2+0.5) * 0.8,
                (sin(GetTime()*5)/2+0.5) * 0.8,
                (sin(GetTime()*7)/2+0.5) * 0.8,
                1);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(FullscreenQuadProgram);
            glBindVertexArray(VAOs[D]);
            glBindTexture(GL_TEXTURE_2D, CameraTexID);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            eglSwapBuffers(
                Display->DisplayDevice,
                Display->Surface);
        }
    }

    return 0;
}
