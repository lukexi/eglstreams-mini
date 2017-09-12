/*
Each display runs on its own thread
Each camera  runs on its own thread
Cameras write using synchronous uploads to a single texture.
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
GLuint CameraTexID;

typedef struct {
    EGLContext Context;
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
    camera_state* CameraState = camera_open("Video0", "/dev/video0", CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }
    uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);

    fps FPS = MakeFPS("Camera");
    while (1) {
        camera_capture(CameraState, CameraBuffer);
        UpdateTexture(CameraThreadState->TexID, CameraWidth, CameraHeight, GL_RGB, CameraBuffer);
        glFlush();
        GLCheck("Camera Thread");
        TickFPS(&FPS);
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

    fps FPS = MakeFPS(Display->EDID->MonitorName);
    while (1) {
        // printf("Drawing %s\n", DisplayName);
        // Draw a texture to the display framebuffer
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, CameraTexID);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        GLCheck("Display Thread");
        TickFPS(&FPS);
    }
}

int main() {
    egl_state* EGL = SetupEGLThreaded();

    // Set up global resources
    FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );
    CameraTexID = CreateTexture(CameraWidth, CameraHeight, CameraChannels);


    // NumDisplays = 1; printf("Forcing number of displays to 1\n");

    // Camera shares root context
    EGLint ContextAttribs[] = { EGL_NONE };
    EGLContext CameraThreadContext = eglCreateContext(
        EGL->DisplayDevice,
        EGL->Config,
        EGL->RootContext,
        ContextAttribs);


    // Launch camera thread
    camera_thread_state* CameraThreadState = malloc(sizeof(camera_thread_state));
    CameraThreadState->Context             = CameraThreadContext;
    CameraThreadState->DisplayDevice       = EGL->DisplayDevice;
    CameraThreadState->TexID               = CameraTexID;
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraThreadState);
    assert(!ResultCode);


    // Launch display threads
    for (int D = 0; D < EGL->DisplaysCount; D++) {
        egl_display* Display = &EGL->Displays[D];
        pthread_t DisplayThread;
        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, Display);
        assert(!ResultCode);
    }
    GLCheck("Hello");

    pthread_join(CameraThread, NULL);

    return 0;
}
