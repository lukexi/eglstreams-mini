#if !defined(GLOBAL_STATE_H)
#define GLOBAL_STATE_H

#include "mvar.h"
#include "egl.h"
#include "camera.h"
#include "dotframe.h"
#include <GL/glew.h>


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


static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;


#endif // GLOBAL_STATE_H
