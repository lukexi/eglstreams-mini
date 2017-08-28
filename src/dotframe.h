#if !defined(DOTFRAME_H)
#define DOTFRAME_H

#include "geom.h"

#define MAX_MOVEMENT_TARGETS 4

typedef struct dot_t {
    int Valid;  // used so we can use { 0 } as a sentinel AND to mark used dots
    vec2 Position;
    vec2 Radius;
    float L;
    float a;
    float b;
    int Consolidated;
    struct dot_t* MovementTargets[MAX_MOVEMENT_TARGETS];
    int MovementTargetsCount;  // -1 if not computed yet
} dot_t;

typedef struct wedge_t {
    dot_t Dots[5];
} wedge_t;

typedef enum detected_by_t {
  STATIONARY,
  MOVING,
  SCRATCH
} detected_by_t;

typedef struct frame_t {
    int Used;
    wedge_t Wedges[4];
    detected_by_t DetectedBy;
    int HasCode;
    int Code;
    float Width;
    float Height;
} frame_t;

int dotframe_update_frames(dot_t* dots, int dots_cnt,
                           frame_t* prev_frames, int prev_frames_cnt,
                           frame_t* cur_frames, int cur_frames_size);

void dot_x_tim_sort(dot_t *dst, const size_t size);

#endif
