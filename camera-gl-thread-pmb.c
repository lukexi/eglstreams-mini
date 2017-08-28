/*
This runs the camera on a thread with its own GL context
and does the copy into the PMB from there.
This also runs pretty terribly, if I remember right.
(similar to camera-thread-gl)
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
#include "global-state.h"

GLuint FullscreenQuadProgram;
#define NUM_TEXTURES 3
GLuint CameraTexIDs[NUM_TEXTURES];

typedef struct {
    EGLContext Context;
    EGLDisplay DisplayDevice;
} camera_thread_state;

int DrawIndex = 0;

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

    const int CameraBufferSize = CameraWidth * CameraHeight * CameraChannels;
    const int TripleBufferSize = CameraBufferSize * NUM_TEXTURES; // Triple buffering

    GLuint PBO;
    glGenBuffers(1, &PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    int Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(PBO, TripleBufferSize, NULL, Flags);
    // uint8_t* CameraBuffer = (uint8_t*)glMapNamedBuffer(PBO, Flags);
    uint8_t* CameraBuffer = (uint8_t*)glMapNamedBufferRange(PBO, 0, TripleBufferSize, Flags);

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    int BufferIndex = 0;
    while (1) {

        size_t BufferOffset = CameraBufferSize * BufferIndex;

        uint8_t* CurrentBuffer = CameraBuffer + BufferOffset;
        // NEWTIME(Capture);
        camera_capture(CameraState, CurrentBuffer);
        // GRAPHTIME(Capture, "-");

        NEWTIME(UpdateTexture);
        glTextureSubImage2D(
            CameraTexIDs[BufferIndex],
            0,
            0, 0,
            CameraWidth, CameraHeight,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            (void*)BufferOffset);
        // glFlush();
        GRAPHTIME(UpdateTexture, "*");
        printf("Uploading into %i\n", BufferIndex);

        BufferIndex = (BufferIndex + 1) % NUM_TEXTURES;
        DrawIndex = (BufferIndex + 4) % NUM_TEXTURES;
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
        // printf("Drawing %s\n", DisplayName);
        // Draw a texture to the display framebuffer
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, CameraTexIDs[DrawIndex]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);

        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        GLCheck("Display Thread");
    }
}


int main() {
    GetTime();

    egl_state* EGL = SetupEGLThreaded();

    FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );

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

    for (int i = 0; i < NUM_TEXTURES; i++) {
        CameraTexIDs[i] = CreateTexture(CameraWidth, CameraHeight, CameraChannels);
    }

    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraThreadState);
    assert(!ResultCode);


    for (int D = 0; D < EGL->DisplaysCount; D++) {
        egl_display* Display = &EGL->Displays[D];
        pthread_t DisplayThread;
        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, Display);
        assert(!ResultCode);
    }

    pthread_join(CameraThread, NULL);

    return 0;
}
