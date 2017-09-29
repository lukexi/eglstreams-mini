/*
This runs the camera in a thread and passes buffers over to the main thread
for copying into a persistent mapped buffer.
The memcpy takes about 1ms, but this otherwise runs well.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <GL/glew.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>

#include "egl.h"
#include "utils.h"

void* AcquireThreadMain(void* Arg) {
    egl_display* Display = Arg;

    while (1) {
        if (Display->PageFlipPending) {
            continue;
        }
        EGLint StreamState = EGLQueryStreamState(Display->DisplayDevice, Display->Stream);
        if (StreamState == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR) {
            NEWTIME(StreamAcquire);
            EGLStreamAcquire(Display);
            ENDTIME(StreamAcquire);
        }

    }
    return NULL;
}

int main() {
    GetTime();

    egl_state* EGL = SetupEGL();
    EnableGLDebug();



    fps MainLoopFPS = MakeFPS("Main Loop");
    fps* DisplayFPS = calloc(EGL->DisplaysCount, sizeof(fps));
    for (int DisplayIndex = 0; DisplayIndex < EGL->DisplaysCount; DisplayIndex++) {
        egl_display* Display = &EGL->Displays[DisplayIndex];

        pthread_t AcquireThread;
        pthread_create(&AcquireThread, NULL, AcquireThreadMain, Display);

        DisplayFPS[DisplayIndex] = MakeFPS(Display->EDID->MonitorName);
    }

    while (1) {

        EGLUpdateVSync(EGL);

        for (int DisplayIndex = 0; DisplayIndex < EGL->DisplaysCount; DisplayIndex++) {
            egl_display* Display = &EGL->Displays[DisplayIndex];

            if (Display->PageFlipPending) {
                continue;
            }

            eglMakeCurrent(Display->DisplayDevice,
                Display->Surface, Display->Surface,
                Display->Context);

            glViewport(0, 0,
                (GLint)Display->Width,
                (GLint)Display->Height);

            // glClearColor(1, 1, 1, 1);
            glClearColor(
                        (sin(GetTime()*3)/2+0.5) * 0.8,
                        (sin(GetTime()*5)/2+0.5) * 0.8,
                        (sin(GetTime()*7)/2+0.5) * 0.8,
                        1);
            glClear(GL_COLOR_BUFFER_BIT);

            NEWTIME(eglFFFSwapBuffers);
            eglSwapBuffers(Display->DisplayDevice, Display->Surface);
            ENDTIME(eglFFFSwapBuffers);

            TickFPS(&DisplayFPS[DisplayIndex]);
        }



        GLCheck("Display Thread");

        TickFPS(&MainLoopFPS);
    }

    return 0;
}
