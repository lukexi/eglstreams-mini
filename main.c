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

static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;

int main() {



    // Camera setup
    uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);
    int IsAsync = 0;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    // EGL setup
    int NumDisplays;
    egl_display* Displays = SetupEGL(&NumDisplays);

    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );
    GLuint FullscreenQuadVAO = CreateFullscreenQuad();

    GLuint CameraTexID = CreateTexture(CameraWidth, CameraHeight, CameraChannels);

    while (1) {

        camera_capture(CameraState, CameraBuffer);
        UpdateTexture(CameraTexID, CameraWidth, CameraHeight, GL_RGB, CameraBuffer);

        for (int D = 0; D < NumDisplays; D++) {
            egl_display* Display = &Displays[D];
            // Set the display's framebuffer as active
            eglMakeCurrent(Display->DisplayDevice,
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
            glBindVertexArray(FullscreenQuadVAO);
            glBindTexture(GL_TEXTURE_2D, CameraTexID);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            eglSwapBuffers(
                Display->DisplayDevice,
                Display->Surface);
        }
    }

    return 0;
}
