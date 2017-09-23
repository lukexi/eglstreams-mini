/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if !defined(EGL_H)
#define EGL_H

#include <stdbool.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "kms.h"
#include <xf86drm.h>

typedef struct {
    drm_edid* EDID;
    int Width;
    int Height;
    EGLSurface Surface;
    EGLContext Context;
    EGLDisplay DisplayDevice;
    EGLConfig Config;
    EGLStreamKHR Stream;
    bool PageFlipPending;
} egl_display;

typedef struct {
    egl_display*    Displays;
    int             DisplaysCount;
    EGLContext      RootContext;
    EGLDisplay      DisplayDevice;
    EGLDeviceEXT    Device;
    EGLConfig       Config;
    int             DRMFD;
    drmEventContext DRMEventContext;
} egl_state;



// One call to do all of the below
egl_state* SetupEGL();

// Components of SetupEGL

// Gets a single graphics card.
// TODO: We'll want this to return all graphics cards.
EGLDeviceEXT GetEglDevice();

// Gets the DRM file descriptor needed to connect EGL to DRM/KMS
int GetDrmFd(EGLDeviceEXT device);

// Gets the "EGLDisplay" object,
// for which there is only one per graphics card.
// Not to be confused with a reference to an actual display, which I
// call an egl_display (which is pretty confusing; FIXME)
EGLDisplay GetEglDisplay(EGLDeviceEXT device, int drmFd);

// Gets an EGLConfig configured for rendering via EGLStreams.
EGLConfig GetEglConfig(EGLDisplay eglDpy);

// Creates a root OpenGL context.
EGLContext GetEglContext(EGLDisplay eglDpy, EGLConfig eglConfig);

// Creates a display for each kms_plane passed in.
egl_display* SetupEGLDisplays(
    EGLDisplay eglDpy,
    EGLConfig eglConfig,
    EGLContext eglContext,
    kms_plane* Planes,
    int NumPlanes);

void InitGLEW();

void GetEglExtensionFunctionPointers(void);

void EGLCheck(const char* name);
const char* EGLStreamStateToString(EGLint streamState);

void EGLUpdateVSync(egl_state* EGL);
void EGLSwapDisplay(egl_display* Display);

#endif /* EGL_H */
