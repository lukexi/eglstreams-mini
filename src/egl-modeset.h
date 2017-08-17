#if !defined(MODESET_H)
#define MODESET_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct modeset_dev {
    struct modeset_dev *next;

    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint8_t *map;

    drmModeModeInfo mode;
    uint32_t fb;
    uint32_t conn;
    uint32_t crtc;
    drmModeCrtc *saved_crtc;
    EGLSurface surface;
    EGLContext context;
    int DisplayIndex;
};


typedef struct modeset_dev display;


display* ModesetDisplays(int* fd_out);
void CleanupDisplays(int fd);
#endif /* MODESET_H */
