#include "egl-display.h"
#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
egl_state* CreateEGLState() {
    egl_state* EGLState = calloc(1, sizeof(egl_state));

    // Modeset all connected monitors
    int fd;
    EGLState->Displays = ModesetDisplays(&fd);

    // Set up EGL
    GetEglExtensionFunctionPointers();

    EGLState->eglDevice  = GetEglDevice();
    EGLState->eglDisplay = GetEglDisplay(EGLState->eglDevice, fd);

    // Create an EGLSurface and EGLContext for each display
    EGLContext SharedContext = EGL_NO_CONTEXT;

    display* Display;
    for (Display = EGLState->Displays; Display; Display = Display->next) {
        EGLContext eglContext;
        Display->surface = SetUpEgl(EGLState->eglDisplay,
            Display->crtc, Display->width, Display->height,
            SharedContext,
            &eglContext);
        Display->context = eglContext;
        // Share the first created EGL context with
        // all subsequently created EGL contexts.
        if (SharedContext == EGL_NO_CONTEXT) {
            SharedContext = eglContext;
        }
    }

    // Make the first EGLContext active
    eglMakeCurrent(EGLState->eglDisplay,
        EGLState->Displays->surface,
        EGLState->Displays->surface,
        EGLState->Displays->context);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK) {
        printf("Could not init glew.\n");
        return 0;
    }
    GLenum GLEWError = glGetError();
    if (GLEWError) {
        printf("GLEW returned error: %i\n", GLEWError);
    }

    return EGLState;
}
