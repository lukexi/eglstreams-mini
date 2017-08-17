#if !defined(CAMERA_H)
#define CAMERA_H
#include <stddef.h>

typedef struct {
    void   *start;  // location where buffer is mapped into process memory
    size_t length;  // length of buffer
} buffer;

typedef struct {
    int                is_async;   // whether camera is in async mode
    const char         *dev_name;  // device file-name (like /dev/video0)
    int                fd;         // file descriptor for opened device
    buffer*            bufs;       // information about buffers
    size_t             num_bufs;   // number of buffers
} camera_state;

void camera_close(camera_state* cam);

// Returns NULL if the device name doesn't exist
camera_state* camera_open(char* dev_name, int width, int height, double fps, int is_async);

// Tries to find the first available camera
camera_state* camera_open_any(int width, int height, double fps, int is_async);

// Opens all available cameras up to num_cameras (which should be the size of the cameras array).
// Returns the number that were successfully opened, which will be placed sequentially into the cameras array.
int camera_open_all(int width, int height, double fps, int is_async, camera_state** cameras, int num_cameras);

void camera_capture(camera_state* cam, void* out_buf);
int camera_set_ctrl_by_name(camera_state* cam, char* name, int value);

#endif // CAMERA_H
