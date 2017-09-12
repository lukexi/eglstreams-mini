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
#include "framebuffer.h"
#include "mvar.h"
#include "texture-buffer.h"

GLuint FullscreenQuadProgram;

#define NUM_FBS 3
typedef struct {
    egl_display* Display;
    GLuint FBs[NUM_FBS];
    GLuint FBTexs[NUM_FBS];
    GLsync RSyncs[NUM_FBS];
    GLsync WSyncs[NUM_FBS];
    int WriteIndex;
} display_fb;

void* DisplayThreadMain(void* ThreadArguments) {

    display_fb* DisplayFB = (display_fb*)ThreadArguments;

    egl_display* Display = DisplayFB->Display;

    char* DisplayName = Display->EDID->MonitorName;
    printf("Starting display %s\n", DisplayName);

    // Set the display's framebuffer as active
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);


    GLuint FramebufferIDs[NUM_FBS];
    glGenFramebuffers(NUM_FBS, FramebufferIDs);

    for (int i = 0; i < NUM_FBS; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, FramebufferIDs[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            DisplayFB->FBTexs[i], 0);
    }


    fps FPS = MakeFPS(Display->EDID->MonitorName);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    while (1) {
        int ReadIndex = (DisplayFB->WriteIndex + 1) % NUM_FBS;

        printf("%s Awaiting sync object %i\n", Display->EDID->MonitorName, ReadIndex);
        WaitSync(DisplayFB->WSyncs[ReadIndex]);
        WaitSync(DisplayFB->RSyncs[ReadIndex]);
        GLCheck(Display->EDID->MonitorName);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferIDs[ReadIndex]);
        glBlitFramebuffer(
            0,0,Display->Width,Display->Height,
            0,0,Display->Width,Display->Height,
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR
            );
        GLCheck("Blit");

        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);

        LockSync(&DisplayFB->RSyncs[ReadIndex]);
        glFlush();
        GLCheck("Display Thread");
        TickFPS(&FPS);

    }
}



int main() {
    egl_state* EGL = SetupEGLThreaded();
    // EnableGLDebug();

    // Set up global resources
    FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/shaded.frag"
        );
    // EGL->DisplaysCount = 1;
    display_fb* DisplayFBs = calloc(EGL->DisplaysCount, sizeof(display_fb));

    // Launch display threads
    pthread_t DisplayThread; // Hold onto the last thread so we can join on it
    for (int D = 0; D < EGL->DisplaysCount; D++) {

        display_fb*  DisplayFB = &DisplayFBs[D];
        egl_display* Display   = &EGL->Displays[D];

        DisplayFB->Display = Display;

        for (int i = 0; i < NUM_FBS; ++i) {
            CreateFramebuffer(GL_RGBA8, Display->Width, Display->Height,
            &DisplayFB->FBTexs[i],
            &DisplayFB->FBs[i]);
        }

        GLCheck("Create");

        int ResultCode = pthread_create(&DisplayThread, NULL, DisplayThreadMain, (void*)DisplayFB);
        assert(!ResultCode);
    }

    fps FPS = MakeFPS("Main thread");
    while (1) {
        for (int D = 0; D < EGL->DisplaysCount; D++) {
            display_fb* DisplayFB = &DisplayFBs[D];

            int WriteIndex = DisplayFB->WriteIndex;
            DisplayFB->WriteIndex = (WriteIndex + 1) % NUM_FBS;
            WaitSync(DisplayFB->WSyncs[WriteIndex]);
            WaitSync(DisplayFB->RSyncs[WriteIndex]);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, DisplayFB->FBs[WriteIndex]);

            glClearColor(
                (sin(GetTime()*3)/2+0.5) * 0.8,
                (sin(GetTime()*5)/2+0.5) * 0.8,
                (sin(GetTime()*7)/2+0.5) * 0.8,
                1);
            glClear(GL_COLOR_BUFFER_BIT);

            printf("%s Locking sync object %i\n", DisplayFB->Display->EDID->MonitorName, WriteIndex);
            LockSync(&DisplayFB->WSyncs[WriteIndex]);
            glFlush();

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        }
        TickFPS(&FPS);
    }

    return 0;
}
