#ifndef GEOM_H
#define GEOM_H

typedef struct vec2 {
    float x;
    float y;
} vec2;

typedef struct mat3 {
    float e[3][3];  // e is for entries
} mat3;

vec2 vec2_plus(vec2 v1, vec2 v2);
vec2 vec2_minus(vec2 v1, vec2 v2);
vec2 vec2_unary_minus(vec2 v);
vec2 vec2_times(float a, vec2 v);
float vec2_angle(vec2 v);
float vec2_len_sq(vec2 v);
float vec2_len(vec2 v);
float vec2_dist_sq(vec2 v1, vec2 v2);
float vec2_dist(vec2 v1, vec2 v2);
vec2 vec2_lerp(vec2 v1, vec2 v2, float a);

// The matrix representing "do m1, then m2". (Opposite of mathematical notation.)
mat3 mat3_sequence(mat3 m1, mat3 m2);
mat3 mat3_sequence3(mat3 m1, mat3 m2, mat3 m3);
mat3 mat3_inv(mat3 m);
mat3 mat3_translation(vec2 v);
mat3 mat3_rotation(float angle);
vec2 apply_homography(mat3 m, vec2 v);
mat3 rigid_motion_from_pairs(vec2 v1_pre, vec2 v1_pos,
                             vec2 v2_pre, vec2 v2_pos);

#endif
