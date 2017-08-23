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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/glew.h>

#include "utils.h"
#include "egl.h"

/* XXX khronos eglext.h does not yet have EGL_DRM_MASTER_FD_EXT */
#if !defined(EGL_DRM_MASTER_FD_EXT)
#define EGL_DRM_MASTER_FD_EXT                   0x333C
#endif

/*
 * Check if 'extension' is present in 'extensionString'.  Note that
 * strstr(3) by itself is not sufficient; see:
 *
 * http://www.opengl.org/registry/doc/rules.html#using
 */
EGLBoolean ExtensionIsSupported(const char *extensionString,
                                const char *extension)
{
    const char *endOfExtensionString;
    const char *currentExtension = extensionString;
    size_t extensionLength;

    if ((extensionString == NULL) || (extension == NULL)) {
        return EGL_FALSE;
    }

    extensionLength = strlen(extension);

    endOfExtensionString = extensionString + strlen(extensionString);

    while (currentExtension < endOfExtensionString) {
        const size_t currentExtensionLength = strcspn(currentExtension, " ");
        if ((extensionLength == currentExtensionLength) &&
            (strncmp(extension, currentExtension,
                     extensionLength) == 0)) {
            return EGL_TRUE;
        }
        currentExtension += (currentExtensionLength + 1);
    }
    return EGL_FALSE;
}


static void *GetProcAddress(const char *functionName)
{
    void *ptr = (void *) eglGetProcAddress(functionName);

    if (ptr == NULL) {
        Fatal("eglGetProcAddress(%s) failed.\n", functionName);
    }

    return ptr;
}


PFNEGLQUERYDEVICESEXTPROC pEglQueryDevicesEXT = NULL;
PFNEGLQUERYDEVICESTRINGEXTPROC pEglQueryDeviceStringEXT = NULL;
PFNEGLGETPLATFORMDISPLAYEXTPROC pEglGetPlatformDisplayEXT = NULL;
PFNEGLGETOUTPUTLAYERSEXTPROC pEglGetOutputLayersEXT = NULL;
PFNEGLCREATESTREAMKHRPROC pEglCreateStreamKHR = NULL;
PFNEGLSTREAMCONSUMEROUTPUTEXTPROC pEglStreamConsumerOutputEXT = NULL;
PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC pEglCreateStreamProducerSurfaceKHR = NULL;

void GetEglExtensionFunctionPointers(void)
{
    pEglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC)
        GetProcAddress("eglQueryDevicesEXT");

    pEglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC)
        GetProcAddress("eglQueryDeviceStringEXT");

    pEglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        GetProcAddress("eglGetPlatformDisplayEXT");

    pEglGetOutputLayersEXT = (PFNEGLGETOUTPUTLAYERSEXTPROC)
        GetProcAddress("eglGetOutputLayersEXT");

    pEglCreateStreamKHR = (PFNEGLCREATESTREAMKHRPROC)
        GetProcAddress("eglCreateStreamKHR");

    pEglStreamConsumerOutputEXT = (PFNEGLSTREAMCONSUMEROUTPUTEXTPROC)
        GetProcAddress("eglStreamConsumerOutputEXT");

    pEglCreateStreamProducerSurfaceKHR = (PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC)
        GetProcAddress("eglCreateStreamProducerSurfaceKHR");
}


/*
 * The EGL_EXT_device_base extension (or EGL_EXT_device_enumeration
 * and EGL_EXT_device_query) let you enumerate the GPUs in the system.
 */
EGLDeviceEXT GetEglDevice(void)
{
    EGLint numDevices, i;
    EGLDeviceEXT *devices = NULL;
    EGLDeviceEXT device = EGL_NO_DEVICE_EXT;
    EGLBoolean ret;

    const char *clientExtensionString =
        eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!ExtensionIsSupported(clientExtensionString,
                              "EGL_EXT_device_base") &&
        (!ExtensionIsSupported(clientExtensionString,
                               "EGL_EXT_device_enumeration") ||
         !ExtensionIsSupported(clientExtensionString,
                               "EGL_EXT_device_query"))) {
        Fatal("EGL_EXT_device base extensions not found.\n");
    }

    /* Query how many devices are present. */
    ret = pEglQueryDevicesEXT(0, NULL, &numDevices);

    if (!ret) {
        Fatal("Failed to query EGL devices.\n");
    }

    if (numDevices < 1) {
        Fatal("No EGL devices found.\n");
    }

    /* Allocate memory to store that many EGLDeviceEXTs. */
    devices = calloc(numDevices, sizeof(EGLDeviceEXT));

    if (devices == NULL) {
        Fatal("Memory allocation failure.\n");
    }

    /* Query the EGLDeviceEXTs. */
    ret = pEglQueryDevicesEXT(numDevices, devices, &numDevices);

    if (!ret) {
        Fatal("Failed to query EGL devices.\n");
    }

    /*
     * Select which EGLDeviceEXT to use.
     *
     * The EGL_EXT_device_query extension defines the functions:
     *
     *   eglQueryDeviceAttribEXT()
     *   eglQueryDeviceStringEXT()
     *
     * as ways to generically query properties of EGLDeviceEXTs, and
     * separate EGL extensions define EGLDeviceEXT attributes that can
     * be queried through those functions.  E.g.,
     *
     * - EGL_NV_device_cuda lets you query the CUDA device ID
     *   (EGL_CUDA_DEVICE_NV of an EGLDeviceEXT.
     *
     * - EGL_EXT_device_drm lets you query the DRM device file
     *   (EGL_DRM_DEVICE_FILE_EXT) of an EGLDeviceEXT.
     *
     * Future extensions could define other EGLDeviceEXT attributes
     * such as PCI BusID.
     *
     * For now, just choose the first device that supports EGL_EXT_device_drm.
     */

    for (i = 0; i < numDevices; i++) {

        const char *deviceExtensionString =
            pEglQueryDeviceStringEXT(devices[i], EGL_EXTENSIONS);

        if (ExtensionIsSupported(deviceExtensionString, "EGL_EXT_device_drm")) {
            device = devices[i];
            // break;
        }
    }

    free(devices);

    if (device == EGL_NO_DEVICE_EXT) {
        Fatal("No EGL_EXT_device_drm-capable EGL device found.\n");
    }

    return device;
}


/*
 * Use the EGL_EXT_device_drm extension to query the DRM device file
 * corresponding to the EGLDeviceEXT.
 */
int GetDrmFd(EGLDeviceEXT device)
{
    const char *deviceExtensionString =
        pEglQueryDeviceStringEXT(device, EGL_EXTENSIONS);

    const char *drmDeviceFile;
    int fd;

    if (!ExtensionIsSupported(deviceExtensionString, "EGL_EXT_device_drm")) {
        Fatal("EGL_EXT_device_drm extension not found.\n");
    }

    drmDeviceFile = pEglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT);

    if (drmDeviceFile == NULL) {
        Fatal("No DRM device file found for EGL device.\n");
    }
    printf("Device file: %s\n", drmDeviceFile);

    fd = open(drmDeviceFile, O_RDWR, 0);

    if (fd < 0) {
        Fatal("Unable to open DRM device file.\n");
    }

    return fd;
}


/*
 * Create an EGLDisplay from the given EGL device.
 */
EGLDisplay GetEglDisplay(EGLDeviceEXT device, int drmFd)
{
    EGLDisplay eglDpy;

    const char *clientExtensionString =
        eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    const char *deviceExtensionString =
        pEglQueryDeviceStringEXT(device, EGL_EXTENSIONS);

    /*
     * Provide the DRM fd when creating the EGLDisplay, so that the
     * EGL implementation can make any necessary DRM calls using the
     * same fd as the application.
     */
    EGLint attribs[] = {
        EGL_DRM_MASTER_FD_EXT,
        drmFd,
        EGL_NONE
    };

    /*
     * eglGetPlatformDisplayEXT requires EGL client extension
     * EGL_EXT_platform_base.
     */
    if (!ExtensionIsSupported(clientExtensionString, "EGL_EXT_platform_base")) {
        Fatal("EGL_EXT_platform_base not found.\n");
    }

    /*
     * EGL_EXT_platform_device is required to pass
     * EGL_PLATFORM_DEVICE_EXT to eglGetPlatformDisplayEXT().
     */

    if (!ExtensionIsSupported(clientExtensionString,
                              "EGL_EXT_platform_device")) {
        Fatal("EGL_EXT_platform_device not found.\n");
    }

    /*
     * Providing a DRM fd during EGLDisplay creation requires
     * EGL_EXT_device_drm.
     */
    if (!ExtensionIsSupported(deviceExtensionString, "EGL_EXT_device_drm")) {
        Fatal("EGL_EXT_device_drm not found.\n");
    }

    /* Get an EGLDisplay from the EGLDeviceEXT. */
    eglDpy = pEglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                       (void*)device, attribs);

    if (eglDpy == EGL_NO_DISPLAY) {
        Fatal("Failed to get EGLDisplay from EGLDevice.");
    }

    if (!eglInitialize(eglDpy, NULL, NULL)) {
        Fatal("Failed to initialize EGLDisplay.");
    }

    return eglDpy;
}


EGLConfig GetEglConfig(EGLDisplay eglDpy) {
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_STREAM_BIT_KHR,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 1,
        EGL_NONE,
    };

    const char *extensionString = eglQueryString(eglDpy, EGL_EXTENSIONS);

    /*
     * EGL_EXT_output_base and EGL_EXT_output_drm are needed to find
     * the EGLOutputLayer for the DRM KMS plane.
     */

    if (!ExtensionIsSupported(extensionString, "EGL_EXT_output_base")) {
        Fatal("EGL_EXT_output_base not found.\n");
    }

    if (!ExtensionIsSupported(extensionString, "EGL_EXT_output_drm")) {
        Fatal("EGL_EXT_output_drm not found.\n");
    }

    /*
     * EGL_KHR_stream, EGL_EXT_stream_consumer_egloutput, and
     * EGL_KHR_stream_producer_eglsurface are needed to create an
     * EGLStream connecting an EGLSurface and an EGLOutputLayer.
     */

    if (!ExtensionIsSupported(extensionString, "EGL_KHR_stream")) {
        Fatal("EGL_KHR_stream not found.\n");
    }

    if (!ExtensionIsSupported(extensionString,
                              "EGL_EXT_stream_consumer_egloutput")) {
        Fatal("EGL_EXT_stream_consumer_egloutput not found.\n");
    }

    if (!ExtensionIsSupported(extensionString,
                              "EGL_KHR_stream_producer_eglsurface")) {
        Fatal("EGL_KHR_stream_producer_eglsurface not found.\n");
    }

    /* Bind full OpenGL as EGL's client API. */

    eglBindAPI(EGL_OPENGL_API);

    /* Find a suitable EGL config. */
    EGLint n = 0;
    EGLConfig eglConfig;
    EGLBoolean ret = eglChooseConfig(eglDpy, configAttribs, &eglConfig, 1, &n);

    if (!ret || !n) {
        Fatal("eglChooseConfig() failed.\n");
    }

    return eglConfig;
}

EGLContext GetEglContext(EGLDisplay eglDpy, EGLConfig eglConfig) {
    /* Create an EGL context using the EGL config. */
    EGLint contextAttribs[] = { EGL_NONE };
    EGLContext eglContext =
        eglCreateContext(eglDpy, eglConfig, EGL_NO_CONTEXT, contextAttribs);

    if (eglContext == NULL) {
        Fatal("eglCreateContext() failed.\n");
    }

    return eglContext;
}

/*
 * Set up EGL to present to a DRM KMS plane through an EGLStream.
 */
egl_display* SetupEGLDisplays(
    EGLDisplay eglDpy,
    EGLConfig eglConfig,
    EGLContext eglContext,
    kms_plane* Planes, int NumPlanes)
{

    EGLBoolean ret;

    egl_display* Displays = malloc(sizeof(egl_display) * NumPlanes);
    for (int PlaneIndex = 0; PlaneIndex < NumPlanes; PlaneIndex++) {
        kms_plane* Plane = &Planes[PlaneIndex];

        EGLAttrib layerAttribs[] = {
            EGL_DRM_PLANE_EXT,
            Plane->PlaneID,
            EGL_NONE,
        };
        printf("Setting up plane ID: %i\n", Plane->PlaneID);

        EGLint streamAttribs[] = { EGL_NONE };

        EGLint surfaceAttribs[] = {
            EGL_WIDTH, Plane->Width,
            EGL_HEIGHT, Plane->Height,
            EGL_NONE
        };

        /* Find the EGLOutputLayer that corresponds to the DRM KMS plane. */
        EGLOutputLayerEXT eglLayer;
        EGLint n = 0;
        ret = pEglGetOutputLayersEXT(eglDpy, layerAttribs, &eglLayer, 1, &n);

        if (!ret || !n) {
            Fatal("Unable to get EGLOutputLayer for plane 0x%08x\n", Plane->PlaneID);
        }

        /* Create an EGLStream. */
        EGLStreamKHR eglStream = pEglCreateStreamKHR(eglDpy, streamAttribs);

        if (eglStream == EGL_NO_STREAM_KHR) {
            Fatal("Unable to create stream.\n");
        }

        /* Set the EGLOutputLayer as the consumer of the EGLStream. */

        ret = pEglStreamConsumerOutputEXT(eglDpy, eglStream, eglLayer);

        if (!ret) {
            Fatal("Unable to create EGLOutput stream consumer.\n");
        }

        /*
         * EGL_KHR_stream defines that normally stream consumers need to
         * explicitly retrieve frames from the stream.  That may be useful
         * when we attempt to better integrate
         * EGL_EXT_stream_consumer_egloutput with DRM atomic KMS requests.
         * But, EGL_EXT_stream_consumer_egloutput defines that by default:
         *
         *   On success, <layer> is bound to <stream>, <stream> is placed
         *   in the EGL_STREAM_STATE_CONNECTING_KHR state, and EGL_TRUE is
         *   returned.  Initially, no changes occur to the image displayed
         *   on <layer>. When the <stream> enters state
         *   EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR, <layer> will begin
         *   displaying frames, without further action required on the
         *   application's part, as they become available, taking into
         *   account any timestamps, swap intervals, or other limitations
         *   imposed by the stream or producer attributes.
         *
         * So, eglSwapBuffers() (to produce new frames) is sufficient for
         * the frames to be displayed.  That behavior can be altered with
         * the EGL_EXT_stream_acquire_mode extension.
         */

        /*
         * Create an EGLSurface as the producer of the EGLStream.  Once
         * the stream's producer and consumer are defined, the stream is
         * ready to use.  eglSwapBuffers() calls for the EGLSurface will
         * deliver to the stream's consumer, i.e., the DRM KMS plane
         * corresponding to the EGLOutputLayer.
         */

        EGLSurface eglSurface = pEglCreateStreamProducerSurfaceKHR(eglDpy, eglConfig,
                                                        eglStream, surfaceAttribs);
        if (eglSurface == EGL_NO_SURFACE) {
            Fatal("Unable to create EGLSurface stream producer.\n");
        }

        /*
         * Make current to the EGLSurface, so that OpenGL rendering is
         * directed to it.
         */
        // EGLContext DisplayContext = eglContext;

        EGLint contextAttribs[] = { EGL_NONE };
        EGLContext DisplayContext =
            eglCreateContext(eglDpy, eglConfig, eglContext, contextAttribs);
        if (DisplayContext == NULL) {
            Fatal("eglCreateContext() failed.\n");
        }

        // ret = eglMakeCurrent(eglDpy, eglSurface, eglSurface, DisplayContext);

        // if (!ret) {
        //     Fatal("Unable to make context and surface current.\n");
        // }

        Displays[PlaneIndex].EDID          = Plane->EDID;
        Displays[PlaneIndex].Width         = Plane->Width;
        Displays[PlaneIndex].Height        = Plane->Height;
        Displays[PlaneIndex].DisplayDevice = eglDpy;
        Displays[PlaneIndex].Surface       = eglSurface;
        Displays[PlaneIndex].Context       = DisplayContext;
        Displays[PlaneIndex].Config        = eglConfig;
    }


    return Displays;
}

void InitGLEW() {
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK) {
        Fatal("Could not init glew.\n");
    }
    GLenum GLEWError = glGetError();
    if (GLEWError) {
        printf("GLEW returned error: %i\n", GLEWError);
    }
}

egl_state* SetupEGL() {
    egl_state* EGL = malloc(sizeof(egl_state));

    // Setup global EGL state
    GetEglExtensionFunctionPointers();

    EGL->Device = GetEglDevice();

    int drmFd = GetDrmFd(EGL->Device);

    EGL->DisplayDevice = GetEglDisplay(EGL->Device, drmFd);
    EGL->Config        = GetEglConfig(EGL->DisplayDevice);
    EGL->RootContext   = GetEglContext(EGL->DisplayDevice, EGL->Config);

    EGLBoolean ret = eglMakeCurrent(EGL->DisplayDevice, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL->RootContext);
    if (!ret) Fatal("Couldn't make main context current\n");

    InitGLEW();

    // Set up EGL state for each connected display
    kms_plane* Planes = SetDisplayModes(drmFd, &EGL->DisplaysCount);
    EGL->Displays     = SetupEGLDisplays(EGL->DisplayDevice, EGL->Config, EGL->RootContext, Planes, EGL->DisplaysCount);

    return EGL;
}


void EGLCheck(const char* name) {
    EGLint err = eglGetError();
    if (err != EGL_SUCCESS) {
        printf("%s: ", name);
        switch (err) {
            case EGL_NOT_INITIALIZED:
                printf("EGL is not initialized, or could not be initialized, for the specified EGL display connection.\n");
                break;
            case EGL_BAD_ACCESS:
                printf("EGL cannot access a requested resource (for example a context is bound in another thread).\n");
                break;
            case EGL_BAD_ALLOC:
                printf("EGL failed to allocate resources for the requested operation.\n");
                break;
            case EGL_BAD_ATTRIBUTE:
                printf("An unrecognized attribute or attribute value was passed in the attribute list.\n");
                break;
            case EGL_BAD_CONTEXT:
                printf("An EGLContext argument does not name a valid EGL rendering context.\n");
                break;
            case EGL_BAD_CONFIG:
                printf("An EGLConfig argument does not name a valid EGL frame buffer configuration.\n");
                break;
            case EGL_BAD_CURRENT_SURFACE:
                printf("The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.\n");
                break;
            case EGL_BAD_DISPLAY:
                printf("An EGLDisplay argument does not name a valid EGL display connection.\n");
                break;
            case EGL_BAD_SURFACE:
                printf("An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.\n");
                break;
            case EGL_BAD_MATCH:
                printf("Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).\n");
                break;
            case EGL_BAD_PARAMETER:
                printf("One or more argument values are invalid.\n");
                break;
            case EGL_BAD_NATIVE_PIXMAP:
                printf("A NativePixmapType argument does not refer to a valid native pixmap.\n");
                break;
            case EGL_BAD_NATIVE_WINDOW:
                printf("A NativeWindowType argument does not refer to a valid native window.\n");
                break;
            case EGL_CONTEXT_LOST:
                printf("A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering.\n");
                break;
        }
        exit(1);
    }
}
