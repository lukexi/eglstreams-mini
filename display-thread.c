/*
Each display runs on its own thread.
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

    fps FPS = MakeFPS(Display->EDID->MonitorName);
    while (1) {
        // Draw a texture to the display framebuffer
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
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
        "shaders/shaded.frag"
        );

    // Launch display threads
    pthread_t DisplayThread; // Hold onto the last thread so we can join on it
    for (int D = 0; D < EGL->DisplaysCount; D++) {
        egl_display* Display = &EGL->Displays[D];
        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, Display);
        assert(!ResultCode);
    }
    GLCheck("Hello");

    pthread_join(DisplayThread, NULL);

    return 0;
}
