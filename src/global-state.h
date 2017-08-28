#if !defined(GLOBAL_STATE_H)
#define GLOBAL_STATE_H

#include "nanovg.h"
#include "mvar.h"
#include "egl.h"
#include "camera.h"
#include "dotframe.h"
#include "config.h"
#include <GL/glew.h>

typedef struct {
    mvar* IlluminationsIn;
    mvar* RecognizedPagesOut;
} render_thread_channels;

typedef struct {
    int PageNumber;
    float Width;
    float Height;
    float Region[4][2];
    char Supporter[40];
} page_info;

typedef struct {
    page_info* PageInfos;
    int        PageInfosCount;
} pages_list;

typedef struct {
    char*         DevNode; // FIXME: this and the below should be in a thread-only data structure
    pthread_t     Thread;  // ^
    camera_config Config;  // ^
    camera_state* Driver;  // /FIXME
    mvar*         BufferMVar;
    GLuint        TexID;
    dot_t*        Dots;
    int           DotsCount;
    frame_t*      Frames;
    int           FramesCount;
} dt_camera_stuff;

static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;

typedef struct {
    render_thread_channels* DemoThreadChannels;
    egl_display*            Displays;
    int                     DisplaysCount;
    NVGcontext*             NVG;
    dt_camera_stuff*        Cameras;
    int                     CamerasCount;
} global_state;

#endif // GLOBAL_STATE_H
