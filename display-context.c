/*
Each display uses its own OpenGL context
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


int main() {
    egl_state* EGL = SetupEGLThreaded();

    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/shaded.frag"
        );

    // Create a VAO for each display, since those aren't shared
    // (c.f. "Container objects")
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


    while (1) {



        for (int D = 0; D < EGL->DisplaysCount; D++) {
            egl_display* Display = &EGL->Displays[D];
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
