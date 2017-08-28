/*
Runs the camera on its own thread, and displays on the main thread.
I expected VSync interference, but instead it seems to lock
to the lowest sync interval
(e.g. with a 30hz and a 60hz monitor, it runs at a smooth locked 30FPS.)
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

static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;

void* CameraThreadMain(void* Args) {
    mvar* CameraMVar = (mvar*)Args;

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    while (1) {
        uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);
        camera_capture(CameraState, CameraBuffer);
        TryWriteMVar(CameraMVar, CameraBuffer);
    }

    return NULL;
}



int main() {

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


    // Start up camera
    mvar* CameraMVar = CreateMVar(free);
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraMVar);
    assert(!ResultCode);

    fps FPS = MakeFPS("Display Thread");
    while (1) {

        uint8_t* CameraBuffer = (uint8_t*)TryReadMVar(CameraMVar);
        if (CameraBuffer) {
            UpdateTexture(CameraTexID, CameraWidth, CameraHeight, GL_RGB, CameraBuffer);
            free(CameraBuffer);
        }


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
        TickFPS(&FPS);
    }

    return 0;
}
