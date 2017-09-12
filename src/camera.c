#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <unistd.h>
#include <turbojpeg.h>
#include "camera.h"



//----------------------------------------------------------------
//
// timing

static uint64_t last_time = 0;

static void tick(char* str) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    uint64_t now = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    if (last_time != 0) {
        // uint64_t time_spent = now - last_time;
        // printf("%s: %lu us\n", str, time_spent);
    }
    last_time = now;
}


typedef enum { NO_RETRY = 0, RETRY = 1 } retry_t;

// ioctl with:
// 1. automatic retries on EINTR
// 2. optionally, automatically retries on EAGAIN
static int xioctl (int fh, int request, void *arg, retry_t should_retry) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (r == -1 && ((errno == EINTR) || (should_retry && errno == EAGAIN)));

    return r;
}

// xioctl, but exit on legit errors
static void xioctl_or_exit (int fh, int request, void *arg, retry_t should_retry, const char* msg, const char* name) {
    int r = xioctl(fh, request, arg, should_retry);

    if (r == -1) {
        fprintf(stderr, "ioctl error %d: %s (%s) (on camera %s)\n", errno, strerror(errno), msg, name);
        exit(EXIT_FAILURE);
    }
}

const enum v4l2_buf_type BUF_TYPE = V4L2_BUF_TYPE_VIDEO_CAPTURE;

static int error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}
void camera_close (camera_state* cam) {
    // Close & deallocate device resources
    xioctl_or_exit(cam->fd, VIDIOC_STREAMOFF, (void *)&BUF_TYPE, RETRY, "VIDIOC_STREAMOFF", cam->dev_name);
    for (int i = 0; i < cam->num_bufs; i++) {
        munmap(cam->bufs[i].start, cam->bufs[i].length);
    }
    close(cam->fd);

    // Free camera_state
    free((void*)cam->dev_name);
    free((void*)cam->bufs);
    free((void*)cam);
}



camera_state* camera_open(char* name, char* dev_name, int width, int height, double fps, int is_async) {
    printf("camera_open got fps %f\n", fps);
    camera_state* cam = (camera_state*)calloc(1, sizeof(camera_state));
    cam->name = strdup(name);
    cam->dev_name = strdup(dev_name);
    cam->is_async = is_async;

    // Open device to get file descriptor (O_NONBLOCK means VIDIOC_DQBUF won't block)
    cam->fd = open(cam->dev_name, O_RDWR | O_NONBLOCK, 0);
    if (cam->fd < 0) {
        printf("While opening device %s: %s\n", dev_name, strerror(errno));
        return NULL;
    }

    // Set image format
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    xioctl_or_exit(cam->fd, VIDIOC_S_FMT, &fmt, RETRY, "VIDIOC_S_FMT", cam->name);
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        error("Camera changed 'MJPG' format to '%.*s'\n",
              4, (char*)&fmt.fmt.pix.pixelformat);
    }
    if ((fmt.fmt.pix.width != width) || (fmt.fmt.pix.height != height)) {
        error("Camera changed %d x %d size to %d x %d\n",
              width, height,
              fmt.fmt.pix.width, fmt.fmt.pix.height);
    }

    if (cam->is_async) cam->num_bufs = 4;
    else               cam->num_bufs = 1;

    // Request allocation of buffers in device memory
    struct v4l2_requestbuffers req = { 0 };
    req.count                      = cam->num_bufs;
    req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory                     = V4L2_MEMORY_MMAP;
    xioctl_or_exit(cam->fd, VIDIOC_REQBUFS, &req, RETRY, "VIDIOC_REQBUFS", cam->name);

    // Map buffers into application memory
    cam->bufs = calloc(cam->num_bufs, sizeof(buffer));
    for (int i = 0; i < cam->num_bufs; i++) {
        struct v4l2_buffer buf = { 0 };
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        xioctl_or_exit(cam->fd, VIDIOC_QUERYBUF, (void*)&buf, RETRY, "VIDIOC_QUERYBUF", cam->name);
        cam->bufs[i].length = buf.length;
        cam->bufs[i].start = mmap(NULL,
            buf.length,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            cam->fd, buf.m.offset);
        if (MAP_FAILED == cam->bufs[i].start) {
            // TODO: cleanup?
            error("During mmap: %s\n", strerror(errno));
        }
    }

    // Set FPS
    struct v4l2_streamparm parm = { 0 };
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1000;
    parm.parm.capture.timeperframe.denominator = fps * parm.parm.capture.timeperframe.numerator;
    xioctl_or_exit(cam->fd, VIDIOC_S_PARM, &parm, RETRY, "VIDIOC_S_PARM", cam->name);
    struct v4l2_fract *tf = &parm.parm.capture.timeperframe;
    printf("fps is set to %f\n", ((double)tf->denominator) / tf->numerator);

    if (cam->is_async) {
        // Enqueue buffers in the driver's incoming capture queue
        for (int i = 0; i < cam->num_bufs; i++) {
            struct v4l2_buffer buf = { 0 };
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index  = i;
            xioctl_or_exit(cam->fd, VIDIOC_QBUF, (void*)&buf, RETRY, "VIDIOC_QBUF", cam->name);
        }
    }

    // Turn on streaming
    int r = xioctl(cam->fd, VIDIOC_STREAMON, (void *)&BUF_TYPE, RETRY);
    if (r == -1) {
        if (errno == 28) {  // "No space left on device"
            camera_close(cam);
            error("While turning on stream: No space left on device\n");
        } else {
            fprintf(stderr, "ioctl error %d: %s (VIDIOC_STREAMON) (on camera %s)\n", errno, strerror(errno), cam->name);
            exit(EXIT_FAILURE);
        }
    }

    //// USING NANOJPEG
    //// Initialize JPEG decoder
    // njInit();

    return cam;
}

// Waits for one of `cam`'s buffers to queued for reading by the
// driver, then dequeues it into `buf`.
static void sync_dqbuf(camera_state* cam, struct v4l2_buffer* buf) {
    tick("sync_dqbuf start");
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(cam->fd, &fds);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    int r;
    do {
        r = select(cam->fd + 1, &fds, NULL, NULL, &tv);
    } while ((r == -1 && (errno == EINTR)));
    if (r == -1) {
        error("During select: %s (camera %s)\n", strerror(errno), cam->name);
    } else if (r == 0) {
        error("During select, timeout! (camera %s)\n", cam->name);
    }

    buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf->memory = V4L2_MEMORY_MMAP;
    xioctl(cam->fd, VIDIOC_DQBUF, buf, NO_RETRY);

    tick("sync_dqbuf done");
}

void camera_capture(camera_state* cam, void* out_buf) {
    tick("camera_capture starts");

    struct v4l2_buffer buf_to_read = { 0 };

    if (cam->is_async) {
        // Dequeue buffers as long as we can to get the most recent queued frame

        struct v4l2_buffer cur_buf = { 0 };
        struct v4l2_buffer prev_buf = { 0 };
        cur_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cur_buf.memory = V4L2_MEMORY_MMAP;

        while (1) {
            tick("VIDIOC_DQBUF start");
            int r = xioctl(cam->fd, VIDIOC_DQBUF, &cur_buf, NO_RETRY);
            tick("VIDIOC_DQBUF done");
            if (r == -1) {  // ioctl error
                if (errno == EAGAIN) {  // not really an error; just no buffer to dequeue
                    if (prev_buf.bytesused) {
                        buf_to_read = prev_buf;
                        break;
                    } else {
                        // no buffer to dequeue and no previous buffer; wait for one
                        // printf("no buffer ready; gotta wait\n");
                        sync_dqbuf(cam, &buf_to_read);
                        break;
                    }
                } else {  // a genuine ioctl error
                    fprintf(stderr, "ioctl error %d: %s (VIDIOC_DQBUF) (on camera %s)\n", errno, strerror(errno), cam->name);
                    exit(EXIT_FAILURE);
                }
            } else {  // ioctl success; cur_buf is a dequeued buffer
                if (prev_buf.bytesused) {
                    int r = xioctl(cam->fd, VIDIOC_QBUF, &prev_buf, RETRY);
                    if (r == -1) {
                        printf("WARNING: Failure to VIDIOC_QBUF (loc 1)\n");
                        printf("Camera: %s\n", cam->name);
                    }
                }
                prev_buf = cur_buf;
            }
        }
    } else {  // camera is sync
        // Enqueue (single) buffer in the driver's incoming capture queue
        struct v4l2_buffer buf = { 0 };
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = 0;
        xioctl(cam->fd, VIDIOC_QBUF, &buf, RETRY);

        // Dequeue buffer
        sync_dqbuf(cam, &buf_to_read);
    }

    tick("gonna decode...");

    void* start = cam->bufs[buf_to_read.index].start;
    int bytesused = buf_to_read.bytesused;

    // printf("JPEG size: %i\n", bytesused);

    tjhandle decompressor = tjInitDecompress();
    int width, height, jpegSubsamp;
    tjDecompressHeader2(decompressor, start, bytesused, &width, &height, &jpegSubsamp);
    tjDecompress2(decompressor, start, bytesused, out_buf, width, 0, height, TJPF_RGB, TJFLAG_FASTDCT);
    tjDestroy(decompressor);
    tick("libjpeg-turbo decode done");

    if (cam->is_async) {
        // Re-enqueue dequeued buffer
        int r = xioctl(cam->fd, VIDIOC_QBUF, &buf_to_read, RETRY);
        if (r == -1) {
            printf("WARNING: Failure to VIDIOC_QBUF (loc 2)\n");
            printf("Camera: %s\n", cam->name);
        }
    }
}


// Returns control id for a name, or -1 if the name is not recognized.
int ctrl_info_by_name(char* name) {
    if (strcmp(name, "brightness") == 0)                     { return V4L2_CID_BRIGHTNESS; }
    if (strcmp(name, "contrast") == 0)                       { return V4L2_CID_CONTRAST; }
    if (strcmp(name, "saturation") == 0)                     { return V4L2_CID_SATURATION; }
    if (strcmp(name, "white_balance_temperature_auto") == 0) { return V4L2_CID_AUTO_WHITE_BALANCE; }
    if (strcmp(name, "gain") == 0)                           { return V4L2_CID_GAIN; }
    if (strcmp(name, "power_line_frequency") == 0)           { return V4L2_CID_POWER_LINE_FREQUENCY; }
    if (strcmp(name, "white_balance_temperature") == 0)      { return V4L2_CID_WHITE_BALANCE_TEMPERATURE; }
    if (strcmp(name, "sharpness") == 0)                      { return V4L2_CID_SHARPNESS; }
    if (strcmp(name, "backlight_compensation") == 0)         { return V4L2_CID_BACKLIGHT_COMPENSATION; }

    if (strcmp(name, "exposure_auto") == 0)          { return V4L2_CID_EXPOSURE_AUTO; }
    if (strcmp(name, "exposure_absolute") == 0)      { return V4L2_CID_EXPOSURE_ABSOLUTE; }
    if (strcmp(name, "exposure_auto_priority") == 0) { return V4L2_CID_EXPOSURE_AUTO_PRIORITY; }
    if (strcmp(name, "pan_absolute") == 0)           { return V4L2_CID_PAN_ABSOLUTE; }
    if (strcmp(name, "tilt_absolute") == 0)          { return V4L2_CID_TILT_ABSOLUTE; }
    if (strcmp(name, "focus_absolute") == 0)         { return V4L2_CID_FOCUS_ABSOLUTE; }
    if (strcmp(name, "focus_auto") == 0)             { return V4L2_CID_FOCUS_AUTO; }
    if (strcmp(name, "zoom_absolute") == 0)          { return V4L2_CID_ZOOM_ABSOLUTE; }

    return -1;
}

int camera_set_ctrl_by_name(camera_state* cam, char* name, int value) {
    int ctrl_id = ctrl_info_by_name(name);
    if (ctrl_id == -1) return 0;

    struct v4l2_control ctrl = { .id = ctrl_id, .value = value };

    int r = xioctl(cam->fd, VIDIOC_S_CTRL, &ctrl, RETRY);
    if (r == -1) {
        printf("WARNING: Failure to VIDIOC_S_CTRL\n");
        printf("Camera: %s\n", cam->name);
    }

    return 1;
}

int camera_get_ctrl_by_name(camera_state* cam, char* name) {
    int ctrl_id = ctrl_info_by_name(name);
    if (ctrl_id == -1) return -1;

    struct v4l2_control ctrl = { .id = ctrl_id };
    xioctl_or_exit(cam->fd, VIDIOC_G_CTRL, &ctrl, RETRY, "VIDIOC_G_CTRL", cam->name);

    return ctrl.value;
}

int camera_get_ctrl_bounds_by_name(camera_state* cam, char* name, int* min, int* max) {
    int ctrl_id = ctrl_info_by_name(name);
    if (ctrl_id == -1) return 0;

    struct v4l2_queryctrl queryctrl;
    queryctrl.id = ctrl_id;
    xioctl_or_exit(cam->fd, VIDIOC_QUERYCTRL, &queryctrl, RETRY, "VIDIOC_QUERYCTRL", cam->name);

    *min = queryctrl.minimum;
    *max = queryctrl.maximum;

    return 1;
}
