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
    egl_state* EGL = SetupEGL();

    // GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
    //     "shaders/basic.vert",
    //     "shaders/shaded.frag"
    //     );

    // GLuint FullscreenQuadVAO = CreateFullscreenQuad();
    fps FPS = MakeFPS("Display Thread");
    while (1) {
        for (int D = 0; D < EGL->DisplaysCount; D++) {
            egl_display* Display = &EGL->Displays[D];
            // Set the display's framebuffer as active
            eglMakeCurrent(Display->DisplayDevice,
                Display->Surface, Display->Surface,
                Display->Context);
            glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

            // Draw a shader to the display framebuffer
            glClearColor(
                (sin(GetTime()*3)/2+0.5) * 0.8,
                (sin(GetTime()*5)/2+0.5) * 0.8,
                (sin(GetTime()*7)/2+0.5) * 0.8,
                1);
            glClear(GL_COLOR_BUFFER_BIT);

            // glUseProgram(FullscreenQuadProgram);
            // glBindVertexArray(FullscreenQuadVAO);
            // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


        }
        for (int D= 0; D < EGL->DisplaysCount; D++) {
            egl_display* Display = &EGL->Displays[D];
            eglMakeCurrent(Display->DisplayDevice,
                Display->Surface, Display->Surface,
                Display->Context);
            eglSwapBuffers(
                Display->DisplayDevice,
                Display->Surface);
        }

        TickFPS(&FPS);
    }

    return 0;
}
