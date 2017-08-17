#if !defined(EGL_DISPLAY_H)
#define EGL_DISPLAY_H

#include "egl-device.h"
#include "egl-modeset.h"



typedef struct {
    display* Displays;

    EGLDeviceEXT eglDevice;

    // An EGLDisplay doesn't correspond to a
    // specific monitor/projector;
    // it's shared among all.
    EGLDisplay eglDisplay;
} egl_state;

// Creates an EGL device,
// sets up all connected monitors,
// and initializes OpenGL via GLEW
egl_state* CreateEGLState();

#endif // EGL_DISPLAY_H
