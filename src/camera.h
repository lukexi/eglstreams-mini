// This is a Demotalk-agnostic camera driver! Please keep it that way cuz it's
// real useful that way.

#if !defined(CAMERA_H)
#define CAMERA_H
#include <stddef.h>

typedef struct {
    void   *start;  // location where buffer is mapped into process memory
    size_t length;  // length of buffer
} buffer;

typedef struct {
    const char*        name;       // a name to identify the camera by
    int                is_async;   // whether camera is in async mode
    const char*        dev_name;   // device file-name (like /dev/video0)
    int                fd;         // file descriptor for opened device
    buffer*            bufs;       // information about buffers
    size_t             num_bufs;   // number of buffers
} camera_state;

void camera_close(camera_state* cam);

// Returns NULL if the device name doesn't exist
camera_state* camera_open(char* name, char* dev_name, int width, int height, double fps, int is_async);

void camera_capture(camera_state* cam, void* out_buf);
int camera_set_ctrl_by_name(camera_state* cam, char* name, int value);
int camera_get_ctrl_by_name(camera_state* cam, char* name);
int camera_get_ctrl_bounds_by_name(camera_state* cam, char* name, int* min, int* max);

#endif // CAMERA_H
