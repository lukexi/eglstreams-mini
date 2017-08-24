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

#define NUM_TEXTURES 5

typedef struct {
    EGLContext Context;
    GLuint TexIDs[NUM_TEXTURES];
    EGLDisplay DisplayDevice;
} camera_thread_state;

#define NEWTIME(name) float __name##Before = GetTime();
#define ENDTIME(name) printf("Took: %.2fms\n", (GetTime() - __name##Before) * 1000);
#define GRAPHTIME(name) printf("%s", #name); Graph((GetTime() - __name##Before) * 1000);

void Graph(int N) {
    for (int i = 0; i < N; ++i) {
        printf("*");
    }
    printf("\n");
}

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
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, TripleBufferSize, 0, Flags);
    uint8_t* CameraBuffer = (uint8_t*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, TripleBufferSize, Flags);

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
        camera_capture(CameraState, CurrentBuffer);

        NEWTIME(UpdateTexture);
        glTextureSubImage2D(
            CameraThreadState->TexIDs[BufferIndex],
            0,
            0, 0,
            CameraWidth, CameraHeight,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            (void*)BufferOffset);
        // glFlush();
        GRAPHTIME(UpdateTexture);
        printf("Uploading into %i\n", BufferIndex);

        BufferIndex = (BufferIndex + 1) % NUM_TEXTURES;
        DrawIndex = BufferIndex;
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
        CameraThreadState->TexIDs[i] = CreateTexture(CameraWidth, CameraHeight, CameraChannels);
    }

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
            glBindTexture(GL_TEXTURE_2D, CameraThreadState->TexIDs[DrawIndex]);
            printf("Drawing from %i\n", DrawIndex);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindTexture(GL_TEXTURE_2D, 0);

            eglSwapBuffers(
                Display->DisplayDevice,
                Display->Surface);
        }
    }

    return 0;
}
