#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <GL/glew.h>
#include <math.h>

#include "egl-display.h"
#include "utils.h"

int main() {

    egl_state* EGLState = CreateEGLState();

    eglSwapInterval(EGLState->eglDisplay, 2);


    while (1) {

        for (display* Display = EGLState->Displays; Display; Display = Display->next) {
            // Set the display's framebuffer as active
            // eglMakeCurrent(EGLState->eglDisplay,
            //     Display->surface, Display->surface,
            //     Display->context);
            glViewport(0, 0, (GLint)Display->width, (GLint)Display->height);

            // Draw a texture to the display framebuffer
            // glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(
                (sin(GetTime()*3)/2+0.5) * 0.8,
                (sin(GetTime()*5)/2+0.5) * 0.8,
                (sin(GetTime()*7)/2+0.5) * 0.8,
                1);
            glClear(GL_COLOR_BUFFER_BIT);

            printf("Trying to swap...\n");
            // glFlush();
            // glFinish();
            int result = eglSwapBuffers(
                EGLState->eglDisplay,
                Display->surface);
            if (!result) {
                EGLCheck("foo");
            }
            printf("Swapped!\n");
        }
    }

    return 0;
}
