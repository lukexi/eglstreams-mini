#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "time.h"
#include "assert.h"

#include "dotframe.h"
#include "geom.h"

#include "stretchy_buffer.h"
#include "stretchy_buffer_helpers.h"


// TODO: This should be settable somehow
int debug_mode = 0;


//----------------------------------------------------------------
//
// timing

clock_t last_time = 0;

static void tick(char* str) {
    if (!debug_mode) return;
    clock_t now = clock();
    if (last_time != 0) {
        double time_spent = (double)(now - last_time) / CLOCKS_PER_SEC;
        printf("%s: %d us\n", str, (int) (time_spent * 1e6));
    }
    last_time = now;
}


//----------------------------------------------------------------
//
//  angles

const double radians_to_degrees = 360 / (2 * M_PI);

double mod(double a, double b) {
    return (a >= 0) ? fmod(a, b) : (fmod(a, b) + b);
}

double turn_angle(double ax, double ay, double bx, double by, double cx, double cy) {
    double th1 = atan2(by - ay, bx - ax) * radians_to_degrees;
    double th2 = atan2(cy - by, cx - bx) * radians_to_degrees;
    return mod(th2 - th1, 360);
}

// The unsigned difference between two angles (in degrees), always 0 - 180.
double angle_diff(double angle1, double angle2) {
  double big_diff = mod(angle2 - angle1, 360);  // in range 0 - 360
  return big_diff <= 180 ? big_diff : 360 - big_diff;
}


//----------------------------------------------------------------
//
//  dots

struct dotPtr_pair_t;

typedef struct df_dot_t {
    double x;
    double y;
    double rx;
    double ry;
    struct df_dot_t** neighbors;  // sb
    struct dotPtr_pair_t* neighbor_pairs;  // sb
    double used_in_frame_of_size;
    double L;
    double a;
    double b;
} df_dot_t;

typedef struct dotPtr_pair_t {
    df_dot_t* d[2];
} dotPtr_pair_t;

// define df_dot_x_tim_sort
#define SORT_NAME df_dot_x
#define SORT_TYPE df_dot_t
#define SORT_CMP(dot1, dot2) ((dot1).x - (dot2).x)
#include "sort.h"


//----------------------------------------------------------------
//
//  wedges

struct wedgePtr_pair_t;

typedef struct df_wedge_t {
    df_dot_t* d[5];
    struct df_wedge_t** connections;
    struct wedgePtr_pair_t* connection_pairs123;
    struct wedgePtr_pair_t* connection_pairs341;
    int is_used_by_some_frame;
} df_wedge_t;

typedef struct wedgePtr_pair_t {
    df_wedge_t* w[2];
} wedgePtr_pair_t;

static df_wedge_t make_wedge(df_dot_t* d1, df_dot_t* d2, df_dot_t* d3, df_dot_t* d4, df_dot_t* d5) {
    return (df_wedge_t) {
        .d = { d1, d2, d3, d4, d5 },
        .connections = NULL,
        .connection_pairs123 = NULL,
        .connection_pairs341 = NULL
    };
}

// define wedge_x_tim_sort
#define SORT_NAME wedge_x
#define SORT_TYPE df_wedge_t
#define SORT_CMP(dot1, dot2) ((dot1).d[3]->x - (dot2).d[3]->x)
#include "sort.h"



//----------------------------------------------------------------
//
//  frames

typedef struct df_frame_t {
    df_wedge_t* w[4];
    double frame_size;
} df_frame_t;

static df_frame_t rotate_frame(df_frame_t frame) {
    return (df_frame_t) { { frame.w[1], frame.w[2], frame.w[3], frame.w[0] }, frame.frame_size };
}

// define frame_size_tim_sort
#define SORT_NAME frame_size
#define SORT_TYPE df_frame_t
#define SORT_CMP(frame1, frame2) ((frame1).frame_size - (frame2).frame_size)
#include "sort.h"



//----------------------------------------------------------------
//
//  decoding things

// code240 maps back and forth between...
//   patterns: 5-digit base-4 numbers which use each digit at least once
//   indices: 0-239

int code240_initialized = 0;
uint8_t code240_pattern_to_index[997];

int code240_pattern_as_int(int a, int b, int c, int d, int e) {
    return 256 * a + 64 * b + 16 * c + 4 * d + e;
}

void code240_init() {
    int index = 0;
    for (int a = 0; a < 4; a++)
    for (int b = 0; b < 4; b++)
    for (int c = 0; c < 4; c++)
    for (int d = 0; d < 4; d++)
    for (int e = 0; e < 4; e++) {
        if ((a == 0 || b == 0 || c == 0 || d == 0 || e == 0) &&
            (a == 1 || b == 1 || c == 1 || d == 1 || e == 1) &&
            (a == 2 || b == 2 || c == 2 || d == 2 || e == 2) &&
            (a == 3 || b == 3 || c == 3 || d == 3 || e == 3)) {
            int pattern = code240_pattern_as_int(a, b, c, d, e);
            code240_pattern_to_index[pattern] = index;
            index++;
        }
    }
}

// There are four colors:
//   0  red     big_a
//   1  orange  big_b
//   2  green   small_a
//   3  purple  small_b

// In these 4-tuples, [0] and [2] are indices of dots which should be big_a and
// small_a in some order, and [1] and [3] are indices of dots which should be
// big_b and small_b in some order.
int all_cross_indices[6][4] = {
    {0, 1, 2, 3},
    {0, 1, 3, 2},
    {0, 2, 1, 3},
    {1, 0, 2, 3},
    {1, 0, 3, 2},
    {2, 0, 3, 1}
};

// Returns code, 0-239
int decode_wedge(wedge_t* wedge) {
    // Find the two dots with closest colors
    int closest_d1, closest_d2;
    float closest_dist_sq = INFINITY;
    for (int d1 = 0; d1 < 5; d1++) {
        for (int d2 = d1 + 1; d2 < 5; d2++) {
            dot_t* dot1 = &wedge->Dots[d1];
            dot_t* dot2 = &wedge->Dots[d2];
            float da = dot1->a - dot2->a;
            float db = dot1->b - dot2->b;
            float dist_sq = da * da + db * db;
            if (dist_sq < closest_dist_sq) {
                closest_d1 = d1;
                closest_d2 = d2;
                closest_dist_sq = dist_sq;
            }
        }
    }

    // Temporarily replace dot1 with the average color ...
    dot_t saved_dot1 = wedge->Dots[closest_d1];
    wedge->Dots[closest_d1].a = (wedge->Dots[closest_d1].a + wedge->Dots[closest_d2].a) / 2;
    wedge->Dots[closest_d1].b = (wedge->Dots[closest_d1].b + wedge->Dots[closest_d2].b) / 2;
    // ... and swap dot2 with the last dot
    dot_t saved_dot2 = wedge->Dots[closest_d2];
    wedge->Dots[closest_d2] = wedge->Dots[4];
    wedge->Dots[4] = saved_dot2;

    // Find the best pairing of dots 0-3
    int best_colors[5];  // An array of five colors, aligned with wedge->Dots
    float best_cross_score = 0;
    for (int i = 0; i < 6; i++) {
        int* cross_indices = &all_cross_indices[i][0];
        // printf("cross_indices %i %i %i %i\n", cross_indices[0], cross_indices[1], cross_indices[2], cross_indices[3]);
        float a_score = fabs(wedge->Dots[cross_indices[0]].a - wedge->Dots[cross_indices[2]].a);
        float b_score = fabs(wedge->Dots[cross_indices[1]].b - wedge->Dots[cross_indices[3]].b);
        float cross_score = a_score + b_score;
        // printf("cross_score %f (%f + %f)\n", cross_score, a_score, b_score);
        if (cross_score > best_cross_score) {
            // Fill in best_colors, remembering to swap big_a/small_a and big_b/small_b if necessary.
            int a_in_order = wedge->Dots[cross_indices[0]].a > wedge->Dots[cross_indices[2]].a;
            best_colors[cross_indices[0]] = a_in_order ? 0 : 2;
            best_colors[cross_indices[2]] = a_in_order ? 2 : 0;
            int b_in_order = wedge->Dots[cross_indices[1]].b > wedge->Dots[cross_indices[3]].b;
            best_colors[cross_indices[1]] = b_in_order ? 1 : 3;
            best_colors[cross_indices[3]] = b_in_order ? 3 : 1;
            best_cross_score = cross_score;
        }
    }

    // Restore the original wedge dot1 ...
    wedge->Dots[closest_d1].a = saved_dot1.a;
    wedge->Dots[closest_d1].b = saved_dot1.b;
    // ... swap dot2 with the last dot ...
    wedge->Dots[4] = wedge->Dots[closest_d2];
    wedge->Dots[closest_d2] = saved_dot2;
    // .. and follow along in best_colors
    best_colors[4] = best_colors[closest_d2];
    best_colors[closest_d2] = best_colors[closest_d1];

    int pattern = code240_pattern_as_int(
        best_colors[0], best_colors[1], best_colors[2], best_colors[3], best_colors[4]
    );
    // printf("colors %i %i %i %i %i\n", best_colors[0], best_colors[1], best_colors[2], best_colors[3], best_colors[4]);
    // printf("pattern %i\n", pattern);
    return code240_pattern_to_index[pattern];
}

int posmod (int a, int b) {
    int ret = a % b;
    if (ret < 0) ret += b;
    return ret;
}

// Tries to decode a frame. If successful, writes the code to the frame and
// possibly rotates the wedges around appropriately.
void decode_frame (frame_t* frame) {
    // Decode wedges
    int wedge_codes[4];
    for (int i = 0; i < 4; i++) {
        wedge_codes[i] = decode_wedge(&frame->Wedges[i]);
    }

    // Try rotations until one works
    for (int rot = 0; rot < 4; rot++) {
        int w0 = wedge_codes[(0 + rot) % 4];
        int w1 = wedge_codes[(1 + rot) % 4];
        int w2 = wedge_codes[(2 + rot) % 4];
        int w3 = wedge_codes[(3 + rot) % 4];
        if (w0 % 2 == 1 && w1 % 2 == 0 && w2 % 2 == 0 && w3 % 2 == 0 &&
            w0 == (w1 + w2 + w3 + 1) % 240) {
            frame->HasCode = 1;

            // Retrieve the segments from 0-119
            int y1 = w1 / 2, y2 = w2 / 2, y3 = w3 / 2;

            // Unscramble
            int x1 = y1;
            int x2 = posmod(y2 - 7 * y1, 120);
            int x3 = posmod(y3 - 11 * y1, 120);

            // Combine
            frame->Code = x1 + 120 * x2 + 120 * 120 * x3;

            // printf("w: %i %i %i %i\n", w0, w1, w2, w3);
            // printf("y: %i %i %i\n", y1, y2, y3);
            // printf("x: %i %i %i\n", x1, x2, x3);
            // printf("code: %i or %i\n", w1 / 2 + 120 * w2 / 2 + 120 * 120 * w3 / 2, frame->Code);

            // Rotate the wedges around appropriately
            wedge_t wedges_copy[4];
            for (int i = 0; i < 4; i++) {
                wedges_copy[i] = frame->Wedges[i];
            }
            for (int i = 0; i < 4; i++) {
                frame->Wedges[i] = wedges_copy[(i + rot) % 4];
            }

            return;
        }
    }

    frame->HasCode = 0;
}



//----------------------------------------------------------------
//
//  sizing frames

// Distance from "head" to "hand" of a wedge, in inches
const float reference_distance = (float)6/4;

// Estimates the distance from a to c assuming that the distance from a to b is reference_distance
float estimate_side (dot_t* a, dot_t* b, dot_t* c) {
    return vec2_dist(a->Position, c->Position) * reference_distance / vec2_dist(a->Position, b->Position);
}

// Estimates a frame's size and writes it to the frame.
void size_frame (frame_t* frame) {
    float wiggle_range = 0.3; // in
    float cur_width = (
        estimate_side(&frame->Wedges[0].Dots[2], &frame->Wedges[0].Dots[4], &frame->Wedges[1].Dots[2]) +
        estimate_side(&frame->Wedges[1].Dots[2], &frame->Wedges[1].Dots[0], &frame->Wedges[0].Dots[2]) +
        estimate_side(&frame->Wedges[2].Dots[2], &frame->Wedges[2].Dots[4], &frame->Wedges[3].Dots[2]) +
        estimate_side(&frame->Wedges[3].Dots[2], &frame->Wedges[3].Dots[0], &frame->Wedges[2].Dots[2])
    ) / 4;
    frame->Width = fabs(frame->Width - cur_width) > wiggle_range ? cur_width : frame->Width;
    float cur_height = (
        estimate_side(&frame->Wedges[0].Dots[2], &frame->Wedges[0].Dots[0], &frame->Wedges[3].Dots[2]) +
        estimate_side(&frame->Wedges[3].Dots[2], &frame->Wedges[3].Dots[4], &frame->Wedges[0].Dots[2]) +
        estimate_side(&frame->Wedges[1].Dots[2], &frame->Wedges[1].Dots[4], &frame->Wedges[2].Dots[2]) +
        estimate_side(&frame->Wedges[2].Dots[2], &frame->Wedges[2].Dots[0], &frame->Wedges[1].Dots[2])
    ) / 4;
    frame->Height = fabs(frame->Height - cur_height) > wiggle_range ? cur_height : frame->Height;
};


//----------------------------------------------------------------
//
//  state

typedef struct df_state_t {
    dot_t* in_dots;        // (ext) dots coming in from the dot detector
    int in_dots_cnt;
    frame_t* prev_frames;  // (ext) frames coming in from the frame_detector
    int prev_frames_cnt;
    frame_t* cur_frames;   // (ext) buffer to write frames going out
    int cur_frames_size;
    int cur_frames_cnt;
} df_state_t;



static int do_wedges_connect (df_wedge_t* wedge1, df_wedge_t* wedge2) {
    for (int i1 = 0; i1 < 5; i1++) {
        for (int i2 = 0; i2 < 5; i2++) {
            if (wedge1->d[i1] == wedge2->d[i2]) return 0;
        }
    }

    df_dot_t *dot1 = wedge1->d[2];
    df_dot_t *dot2 = wedge1->d[4];
    df_dot_t *dot3 = wedge2->d[0];
    df_dot_t *dot4 = wedge2->d[2];
    return angle_diff(turn_angle(dot1->x, dot1->y, dot2->x, dot2->y, dot3->x, dot3->y), 0) < 10
        && angle_diff(turn_angle(dot2->x, dot2->y, dot3->x, dot3->y, dot4->x, dot4->y), 0) < 10;
}


static df_dot_t* load_dots (dot_t* ext_dots, int dots_cnt) {
    df_dot_t* dots = NULL;

    for (int i = 0; i < dots_cnt; i++) {
        dot_t* dot = &ext_dots[i];
        if (!dot->Valid) {
            continue;
        }
        df_dot_t* df_dot = sb_add(dots, 1);
        df_dot->x = dot->Position.x;
        df_dot->y = dot->Position.y;
        df_dot->rx = dot->Radius.x;
        df_dot->ry = dot->Radius.y;
        df_dot->L = dot->L;
        df_dot->a = dot->a;
        df_dot->b = dot->b;
        df_dot->neighbors = NULL;
        df_dot->neighbor_pairs = NULL;
        df_dot->used_in_frame_of_size = HUGE_VAL;
    }

    tick("load_dots");
    return dots;
}

static void get_dot_neighbors (df_dot_t* dots) {
    int num_neighbors = 0;

    SB_FOR_DISTINCT_PAIR_PTR(df_dot_t* dot1, df_dot_t* dot2, dots, {
        double dx = dot2->x - dot1->x;
        if (dx > 8 * dot1->rx) break;
        double dy = dot2->y - dot1->y;
        if (fabs(dy) < 8 * dot1->ry) {
            double dist_sq = dx * dx + dy * dy;
            double threshold = 2 * (fmax(dot1->rx, dot1->ry) + fmax(dot2->rx, dot2->ry));
            if (dist_sq < threshold * threshold) {
                sb_push(dot1->neighbors, dot2);
                sb_push(dot2->neighbors, dot1);
                num_neighbors += 2;
            }
        }
    })

    tick("get_dot_neighbors");
    if (debug_mode) printf("  %i neighbors\n", num_neighbors);
}

static void get_dot_neighbor_pairs (df_dot_t* dots) {
    SB_FOR_PTR(df_dot_t* dot2, dots, {
        SB_FOR_DISTINCT_PAIR(df_dot_t* dot1, df_dot_t* dot3, dot2->neighbors, {
            double turn = turn_angle(dot1->x, dot1->y, dot2->x, dot2->y, dot3->x, dot3->y);
            if (angle_diff(turn, 0) < 30) {
                sb_push(dot1->neighbor_pairs, ((struct dotPtr_pair_t) { dot2, dot3 }));
                sb_push(dot3->neighbor_pairs, ((struct dotPtr_pair_t) { dot2, dot1 }));
            }
        })
    })

    tick("get_dot_neighbor_pairs");
}

static df_wedge_t* get_wedges (df_dot_t* dots) {
    df_wedge_t* wedges = NULL;
    int num_wedges = 0;

    SB_FOR_PTR(df_dot_t* dot3, dots, {
        SB_FOR_DISTINCT_PAIR(dotPtr_pair_t pair1, dotPtr_pair_t pair2, dot3->neighbor_pairs, {
            df_dot_t* pair1_dot2 = pair1.d[1];
            df_dot_t* pair2_dot2 = pair2.d[1];
            double turn = turn_angle(pair1_dot2->x, pair1_dot2->y, dot3->x, dot3->y, pair2_dot2->x, pair2_dot2->y);
            if (angle_diff(turn, 90) < 30) {
                sb_push(wedges, make_wedge(pair1.d[1], pair1.d[0], dot3, pair2.d[0], pair2.d[1]));
                num_wedges++;
            } else if (angle_diff(turn, -90) < 30) {
                sb_push(wedges, make_wedge(pair2.d[1], pair2.d[0], dot3, pair1.d[0], pair1.d[1]));
                num_wedges++;
            }
            // TEMPORARY HACK to suppress ridiculous wedge-counts...
            if (num_wedges > 9000) {
                printf("  num_wedges is over 9000! calling it quits\n");
                goto quits;
            }
        })
    })
    quits:

    tick("get_wedges");
    if (debug_mode) printf("  %i wedges\n", num_wedges);
    return wedges;
}

static void get_wedge_connections (df_wedge_t* wedges, double range) {
    int num_connections = 0;

    for (int w1 = 0; w1 < sb_count(wedges); w1++) {
        df_wedge_t* wedge1 = &wedges[w1];
        // if (wedge1->is_used_by_some_frame) continue;
        double wedge1x = wedge1->d[3]->x;
        wedge1->connections = NULL;

        // Go forward until you pass the range
        for (int w2 = w1 + 1; w2 < sb_count(wedges); w2++) {
            df_wedge_t* wedge2 = &wedges[w2];
            // if (wedge2->is_used_by_some_frame) continue;
            if (wedge2->d[3]->x - wedge1x > range) break;
            if (fabs(wedge2->d[3]->y - wedge1->d[3]->y) < range
                    && do_wedges_connect(wedge1, wedge2)) {
                sb_push(wedge1->connections, wedge2);
                num_connections++;
            }
        }

        // Go backward until you pass the range
        for (int w2 = w1 - 1; w2 >= 0; w2--) {
            df_wedge_t* wedge2 = &wedges[w2];
            if (wedge2->d[3]->x - wedge1x < -range) break;
            if (fabs(wedge2->d[3]->y - wedge1->d[3]->y) < range
                    && do_wedges_connect(wedge1, wedge2)) {
                sb_push(wedge1->connections, wedge2);
                num_connections++;
            }
        }
    }

    tick("get_wedge_connections");
    if (debug_mode) printf("  %i connections\n", num_connections);
}

static void get_wedge_connection_pairs (df_wedge_t* wedges) {
    int num_connection_pairs = 0;

    SB_FOR_PTR(df_wedge_t* wedge1, wedges, {
        wedge1->connection_pairs123 = NULL;
        SB_FOR(df_wedge_t* wedge2, wedge1->connections, {
            if (wedge2 > wedge1) {
                SB_FOR(df_wedge_t* wedge3, wedge2->connections, {
                    if (wedge3 > wedge1) {
                        sb_push(wedge1->connection_pairs123, ((wedgePtr_pair_t) { wedge2, wedge3 }));
                        num_connection_pairs++;
                    }
                })
            }
        })
    })

    SB_FOR_PTR(df_wedge_t* wedge3, wedges, {
        wedge3->connection_pairs341 = NULL;
        SB_FOR(df_wedge_t* wedge4, wedge3->connections, {
            SB_FOR(df_wedge_t* maybe_wedge1, wedge4->connections, {
                if (wedge3 > maybe_wedge1 && wedge4 > maybe_wedge1) {
                    sb_push(wedge3->connection_pairs341, ((wedgePtr_pair_t) { wedge4, maybe_wedge1 }));
                    num_connection_pairs++;
                }
            })
        })
    })

    tick("get_wedge_connection_pairs");
    if (debug_mode) printf("  %i connection pairs\n", num_connection_pairs);
}

static df_frame_t* get_raw_frames (df_wedge_t* wedges) {
    df_frame_t* raw_frames = NULL;
    SB_FOR_PTR(df_wedge_t* wedge1, wedges, {
        SB_FOR(wedgePtr_pair_t wedge2_wedge3, wedge1->connection_pairs123, {
            df_wedge_t* wedge3 = wedge2_wedge3.w[1];
            SB_FOR(wedgePtr_pair_t wedge4_wedge5, wedge3->connection_pairs341, {
                df_wedge_t* wedge5 = wedge4_wedge5.w[1];
                if (wedge1 == wedge5) {
                    df_wedge_t* wedge2 = wedge2_wedge3.w[0];
                    df_wedge_t* wedge4 = wedge4_wedge5.w[0];
                    df_dot_t dot1 = *wedge1->d[2];
                    df_dot_t dot2 = *wedge2->d[2];
                    df_dot_t dot3 = *wedge3->d[2];
                    df_dot_t dot4 = *wedge4->d[2];

                    double frame_size = (
                        fmax(dot1.x, fmax(dot2.x, fmax(dot3.x, dot4.x))) -
                        fmin(dot1.x, fmin(dot2.x, fmin(dot3.x, dot4.x))) +
                        fmax(dot1.y, fmax(dot2.y, fmax(dot3.y, dot4.y))) -
                        fmin(dot1.y, fmin(dot2.y, fmin(dot3.y, dot4.y))));

                    sb_push(raw_frames,
                        ((df_frame_t) { wedge1, wedge2, wedge3, wedge4, frame_size }));
                }
            })
        })
    })

    tick("get_raw_frames");
    if (debug_mode) printf("  %i frames\n", sb_count(raw_frames));

    return raw_frames;
}

static int wedge_has_dot_used_by_frame_smaller_than (df_wedge_t *wedge, double size) {
    for (int d = 1; d < 5; d++) {
        if (size > wedge->d[d]->used_in_frame_of_size + 1) return 1;
    }
    return 0;
}

static int frame_had_dot_used_by_frame_smaller_than (df_frame_t *frame, double size) {
    for (int w = 0; w < 4; w++) {
        if (wedge_has_dot_used_by_frame_smaller_than(frame->w[w], size)) return 1;
    }
    return 0;
}

static void mark_frame_as_used (df_frame_t *frame, double size) {
    for (int w = 0; w < 4; w++) {
        df_wedge_t* wedge = frame->w[w];
        wedge->is_used_by_some_frame = 1;
        for (int d = 1; d < 5; d++) {
            wedge->d[d]->used_in_frame_of_size = size;
        }
    }
}

static void append_minimal_frames (df_state_t* state, df_frame_t* raw_frames) {
    int num_frames = 0;
    SB_FOR_PTR(df_frame_t* frame, raw_frames, {
        // Check if the frame has a used dot
        if (!frame_had_dot_used_by_frame_smaller_than(frame, frame->frame_size)) {
            mark_frame_as_used(frame, frame->frame_size);

            // Add frame
            frame_t* cur_frame = &state->cur_frames[state->cur_frames_cnt];
            cur_frame->DetectedBy = SCRATCH;
            for (int w = 0; w < 4; w++) {
                for (int d = 0; d < 5; d++) {
                    df_dot_t* in_dot = frame->w[w]->d[d];
                    dot_t* out_dot = &cur_frame->Wedges[w].Dots[d];
                    out_dot->Position.x = in_dot->x;
                    out_dot->Position.y = in_dot->y;
                    out_dot->Radius.x = in_dot->rx;
                    out_dot->Radius.y = in_dot->ry;
                    out_dot->L = in_dot->L;
                    out_dot->a = in_dot->a;
                    out_dot->b = in_dot->b;
                }
                // printf("decoded wedge: %i\n", decode_wedge(&cur_frame->Wedges[w]));
            }

            decode_frame(cur_frame);

            if (cur_frame->HasCode) {
                state->cur_frames_cnt++;
                num_frames++;
            }
        }
    })

    tick("append_minimal_frames");
    if (debug_mode) printf("  %i frames found\n", num_frames);
}

static df_wedge_t* wedges_without_used_dots (df_wedge_t* wedges) {
    df_wedge_t* good_wedges = NULL;

    SB_FOR_PTR(df_wedge_t* wedge, wedges, {
        if (!wedge->is_used_by_some_frame) {
            if (!wedge_has_dot_used_by_frame_smaller_than(wedge, HUGE_VAL)) {
                sb_push(good_wedges, *wedge);
            }
        }
    })

    tick("wedges_without_used_dots");
    if (debug_mode) printf("  %i wedges\n", sb_count(good_wedges));
    return good_wedges;
}

// Finds all minimal frames made of unused dots in the SB `wedges` which are
// made of wedges within `range` of each other. Uses up dots & wedges in the
// process.
static void append_frames_from_wedges(df_state_t* state, df_wedge_t* wedges, float range) {
    get_wedge_connections(wedges, range);

    get_wedge_connection_pairs(wedges);

    df_frame_t* raw_frames = get_raw_frames(wedges);

    frame_size_tim_sort(raw_frames, sb_count(raw_frames));
    tick("frame_size_tim_sort");

    append_minimal_frames(state, raw_frames);
    sb_free(raw_frames);
}

static dot_t* in_dot_near_pt(df_state_t* state, vec2 v, float dist) {
    float dist_sq = dist * dist;

    for (int i = 0; i < state->in_dots_cnt; i++) {
        dot_t* dot = &state->in_dots[i];
        if (!dot->Valid) continue;
        if (vec2_dist_sq(dot->Position, v) <= dist_sq) return dot;
    }

    return NULL;
};

static void append_stationary_frames(df_state_t* state) {
    for (int i = 0; i < state->prev_frames_cnt; i++) {
        frame_t* prev_frame = &state->prev_frames[i];

        int num_matches = 0;
        for (int w = 0; w < 4; w++) {
            for (int d = 0; d < 5; d++) {
                dot_t* frame_dot = &prev_frame->Wedges[w].Dots[d];
                dot_t* nearby_dot = in_dot_near_pt(state, frame_dot->Position, 1);
                if (nearby_dot) {
                    num_matches++;
                    frame_dot->Position = nearby_dot->Position;
                    nearby_dot->Valid = 0;  // TODO: we're claiming dots even if num_matches <= 10?
                }
            }
        }

        if (num_matches > 5) {
            // printf("%i matches, pushing to frames!\n", num_matches);
            state->cur_frames[state->cur_frames_cnt] = *prev_frame;
            state->cur_frames[state->cur_frames_cnt].DetectedBy = STATIONARY;
            state->cur_frames_cnt++;
            prev_frame->Used = 1;
            // printf("stationary frame persisted!\n");
        } else {
            prev_frame->Used = 0;
            // printf("stationary frame NOT persisted!\n");
        }
    }

    tick("append_stationary_frames");
}

static void compute_movement_targets(df_state_t* state, dot_t* prev_dot, float dist) {
    if (prev_dot->MovementTargetsCount >= 0) return;

    float dist_sq = dist * dist;

    prev_dot->MovementTargetsCount = 0;

    for (int i = 0; i < state->in_dots_cnt; i++) {
        if (prev_dot->MovementTargetsCount == MAX_MOVEMENT_TARGETS) break;

        dot_t* dot = &state->in_dots[i];
        if (!dot->Valid) continue;
        if (vec2_dist_sq(dot->Position, prev_dot->Position) <= dist_sq) {
            prev_dot->MovementTargets[prev_dot->MovementTargetsCount] = dot;
            prev_dot->MovementTargetsCount++;
        }
    }

    // printf("compute_movement_targets found %i dots\n", prev_dot->MovementTargetsCount);
};

static void append_moving_frames(df_state_t* state) {
    srand(time(NULL));

    for (int prev_frame_i = 0; prev_frame_i < state->prev_frames_cnt; prev_frame_i++) {
        frame_t* prev_frame = &state->prev_frames[prev_frame_i];
        if (prev_frame->Used == 1) continue;

        float lowest_cost = INFINITY;
        frame_t temp_frame = *prev_frame;  // A frame under construction
        frame_t cur_frame = *prev_frame;   // The best-matching frame

        dot_t* temp_frame_dots_used[20];
        dot_t* cur_frame_dots_used[20];
        int cur_frame_dots_used_cnt;

        // printf("trying a moving frame!\n");

        // We keep an array of the frame's dots. If we discover that a dot has
        // no movement targets, we remove it. The array will be reshuffled
        // during the random selection process, so the three dots we care about
        // are in the front.
        dot_t* prev_frame_dots[20];
        int prev_frame_dots_cnt = 20;
        for (int w = 0; w < 4; w++) {
            for (int d = 0; d < 5; d++) {
                prev_frame_dots[w * 5 + d] = &prev_frame->Wedges[w].Dots[d];
                prev_frame_dots[w * 5 + d]->MovementTargetsCount = -1;
            }
        }



        // printf("choose some dots\n");
        for (int choose_some_dots = 0; choose_some_dots < 1000; choose_some_dots++) {
            // In each trial, we pick three dots at random and try to find matches
            for (int j = 0; j < 2; j++) {
                pick_new_choice:

                if (prev_frame_dots_cnt < 2) {
                    // We've run out of useful dots! Quit this frame
                    // printf("no more useful dots :(\n");
                    goto next_frame;
                }
                int next_choice = j + rand() % (prev_frame_dots_cnt - j);

                compute_movement_targets(state, prev_frame_dots[next_choice], 20);
                assert(prev_frame_dots[next_choice]->MovementTargetsCount >= 0);

                if (prev_frame_dots[next_choice]->MovementTargetsCount == 0) {
                    // It's useless. Fill it in with the last member of the
                    // useful part of the array.
                    prev_frame_dots[next_choice] = prev_frame_dots[prev_frame_dots_cnt - 1];
                    prev_frame_dots_cnt--;
                    goto pick_new_choice;
                }

                // It's useful! Swap it to the front.
                dot_t* old_dot_in_front = prev_frame_dots[j];
                prev_frame_dots[j] = prev_frame_dots[next_choice];
                prev_frame_dots[next_choice] = old_dot_in_front;
            }

            float prev_dot_dist_sq = vec2_dist_sq(prev_frame_dots[0]->Position, prev_frame_dots[1]->Position);

            // printf("%i, %i\n", prev_frame_dots[0]->MovementTargetsCount, prev_frame_dots[1]->MovementTargetsCount);
            // Check all combinations of targets
            for (int target_i0 = 0; target_i0 < prev_frame_dots[0]->MovementTargetsCount; target_i0++) {
                dot_t* target_dot0 = prev_frame_dots[0]->MovementTargets[target_i0];
                for (int target_i1 = 0; target_i1 < prev_frame_dots[1]->MovementTargetsCount; target_i1++) {
                    dot_t* target_dot1 = prev_frame_dots[1]->MovementTargets[target_i1];
                    float target_dot_dist_sq = vec2_dist_sq(target_dot0->Position, target_dot1->Position);
                    if (fabs(target_dot_dist_sq - prev_dot_dist_sq) < 10) {
                        mat3 rigid_motion = rigid_motion_from_pairs(prev_frame_dots[0]->Position, target_dot0->Position,
                                                                    prev_frame_dots[1]->Position, target_dot1->Position);
                        // printf("delta = %f\n", target_dot_dist_sq - prev_dot_dist_sq);
                        // printf("%f %f %f\n", rigid_motion.e[0][0], rigid_motion.e[0][1], rigid_motion.e[0][2]);
                        // printf("%f %f %f\n", rigid_motion.e[1][0], rigid_motion.e[1][1], rigid_motion.e[1][2]);

                        float cost = 0;
                        int temp_frame_dots_used_cnt = 0;
                        for (int check_dot_w = 0; check_dot_w < 4; check_dot_w++) {
                            for (int check_dot_d = 0; check_dot_d < 5; check_dot_d++) {
                                dot_t* check_dot = &prev_frame->Wedges[check_dot_w].Dots[check_dot_d];

                                vec2 est = apply_homography(rigid_motion, check_dot->Position);
                                // printf("est = %f, %f\n", est.x, est.y);
                                const float search_range = 6;
                                dot_t* nearby_dot = in_dot_near_pt(state, est, search_range);

                                if (nearby_dot) {
                                    cost += vec2_dist_sq(est, nearby_dot->Position);
                                    vec2 new_position = vec2_lerp(est, nearby_dot->Position, 0.25);
                                    temp_frame.Wedges[check_dot_w].Dots[check_dot_d] = (dot_t) {1, new_position, check_dot->Radius};
                                    temp_frame_dots_used[temp_frame_dots_used_cnt] = nearby_dot;
                                    temp_frame_dots_used_cnt++;
                                } else {
                                    cost += 2 * search_range * search_range;
                                    temp_frame.Wedges[check_dot_w].Dots[check_dot_d] = (dot_t) {1, est, check_dot->Radius};
                                }
                            }
                        }
                        // printf("%i / %i matches, %f / %f\n", temp_frame_dots_used_cnt, prev_frame_dots_cnt, cost, lowest_cost);
                        if (cost < lowest_cost) {
                            // printf("new winner!\n");
                            cur_frame = temp_frame;
                            for (int i = 0; i < temp_frame_dots_used_cnt; i++) {
                                assert(temp_frame_dots_used[i] != NULL);
                                cur_frame_dots_used[i] = temp_frame_dots_used[i];
                            }
                            cur_frame_dots_used_cnt = temp_frame_dots_used_cnt;
                            lowest_cost = cost;
                        }
                    }
                }
            }
        }

        if (cur_frame_dots_used_cnt > 4) {
            state->cur_frames[state->cur_frames_cnt] = cur_frame;
            state->cur_frames[state->cur_frames_cnt].DetectedBy = MOVING;
            state->cur_frames_cnt++;
            // TODO: These lines seg-fault, and don't seem to be necessary??? wtf
            for (int i = 0; i < cur_frame_dots_used_cnt; i++)
                cur_frame_dots_used[i]->Valid = 0;
            printf("moving frame persisted!\n");
        } else {
            printf("moving frame NOT persisted! %i\n", cur_frame_dots_used_cnt);
        }

        next_frame: {}
    }

    tick("append_moving_frames");
}

int dotframe_update_frames(dot_t* in_dots, int in_dots_cnt,
                           frame_t* prev_frames, int prev_frames_cnt,
                           frame_t* cur_frames, int cur_frames_size) {
    tick("start");

    if (!code240_initialized) {
        code240_init();
        code240_initialized = 1;
        tick("code240_init");
    }

    df_state_t state_alloc;
    df_state_t* state = &state_alloc;
    state->in_dots = in_dots;
    state->in_dots_cnt = in_dots_cnt;
    state->prev_frames = prev_frames;
    state->prev_frames_cnt = prev_frames_cnt;
    state->cur_frames = cur_frames;
    state->cur_frames_size = cur_frames_size;
    state->cur_frames_cnt = 0;

    append_stationary_frames(state);
    append_moving_frames(state);

    df_dot_t* dots = load_dots(state->in_dots, state->in_dots_cnt);

    df_dot_x_tim_sort(dots, sb_count(dots));
    tick("df_dot_x_tim_sort");
    // Dot pointers should be fixed from here forward.

    get_dot_neighbors(dots);
    get_dot_neighbor_pairs(dots);

    df_wedge_t* wedges = get_wedges(dots);

    wedge_x_tim_sort(wedges, sb_count(wedges));
    tick("wedge_x_tim_sort");
    // Wedge pointers should be fixed from here forward.

    if (debug_mode) printf("ROUND 1\n");
    append_frames_from_wedges(state, wedges, 150);

    if (debug_mode) printf("ROUND 2\n");
    df_wedge_t* wedges2 = wedges_without_used_dots(wedges);
    append_frames_from_wedges(state, wedges2, 10000);

    // if (debug_mode) printf("  %i frames total\n", sb_count(frames));

    for (int i = 0; i < state->cur_frames_cnt; i++) {
        size_frame(&state->cur_frames[i]);
    }

    // CLEAN-UP: All heap allocation is done via stretchy-buffers. If you can match every
    // buffer with a sb_free call, you'll be set.

    SB_FOR_PTR(df_dot_t* dot, dots, {
        sb_free(dot->neighbors);
        sb_free(dot->neighbor_pairs);
    })
    sb_free(dots);

    SB_FOR_PTR(df_wedge_t* wedge, wedges, {
        sb_free(wedge->connections);
        sb_free(wedge->connection_pairs123);
        sb_free(wedge->connection_pairs341);
    })
    sb_free(wedges);
    sb_free(wedges2);

    tick("freeing everything");

    return state->cur_frames_cnt;
}
