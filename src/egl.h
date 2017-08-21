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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "kms.h"

typedef struct {
    int Width;
    int Height;
    EGLSurface Surface;
    EGLContext Context;
    EGLDisplay DisplayDevice;
} egl_display;

// One call to do all of the below
egl_display* SetupEGL(int* NumDisplays);

// Components of SetupEGL
EGLDeviceEXT GetEglDevice(void);

int GetDrmFd(EGLDeviceEXT device);

EGLDisplay GetEglDisplay(EGLDeviceEXT device, int drmFd);

EGLConfig GetEglConfig(EGLDisplay eglDpy);
EGLContext GetEglContext(EGLDisplay eglDpy, EGLConfig eglConfig);

egl_display* GetEglDisplays(
    EGLDisplay eglDpy,
    EGLConfig eglConfig,
    EGLContext eglContext,
    kms_plane* Planes,
    int NumPlanes);

void InitGLEW();

#endif /* EGL_H */
