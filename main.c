#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <GL/glew.h>

#include "egl-display.h"
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
    uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);
    int IsAsync = 1;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    egl_state* EGLState = CreateEGLState();


    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );

    // Store fullscreen quad geometry
    GLuint FullscreenQuadVAO = CreateFullscreenQuad();

    GLuint CameraTexID = CreateTexture(CameraWidth, CameraHeight, CameraChannels);

    while (1) {
        camera_capture(CameraState, CameraBuffer);
        UpdateTexture(CameraTexID, CameraWidth, CameraHeight, GL_RGB, CameraBuffer);

        for (display* Display = EGLState->Displays; Display; Display = Display->next) {
            // Set the display's framebuffer as active
            eglMakeCurrent(EGLState->eglDisplay,
                Display->surface, Display->surface,
                Display->context);
            glViewport(0, 0, (GLint)Display->width, (GLint)Display->height);

            // Draw a texture to the display framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(1,0,0,1);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(FullscreenQuadProgram);
            glBindVertexArray(FullscreenQuadVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, CameraTexID);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            eglSwapBuffers(
                EGLState->eglDisplay,
                Display->surface);
        }
    }

    return 0;
}
