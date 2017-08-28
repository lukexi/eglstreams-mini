#include "geom.h"

#include "math.h"

//----------------------------------------------------------------
//
// vectors

vec2 vec2_plus(vec2 v1, vec2 v2) { return (vec2){ v1.x + v2.x, v1.y + v2.y }; }
vec2 vec2_minus(vec2 v1, vec2 v2) { return (vec2){ v1.x - v2.x, v1.y - v2.y }; }
vec2 vec2_unary_minus(vec2 v) { return (vec2){ -v.x, -v.y }; }
vec2 vec2_times(float a, vec2 v) { return (vec2){ a * v.x, a * v.y }; }
float vec2_angle(vec2 v) { return atan2(v.y, v.x); }
float vec2_len_sq(vec2 v) { return v.x * v.x + v.y * v.y; }
float vec2_len(vec2 v) { return sqrt(vec2_len_sq(v)); }
float vec2_dist_sq(vec2 v1, vec2 v2) { return vec2_len_sq(vec2_minus(v1, v2)); }
float vec2_dist(vec2 v1, vec2 v2) { return vec2_len(vec2_minus(v1, v2)); }
vec2 vec2_lerp(vec2 v1, vec2 v2, float a) {
    return vec2_plus(vec2_times(1 - a, v1), vec2_times(a, v2));
}

//----------------------------------------------------------------
//
// matrices

mat3 mat3_sequence(mat3 m1, mat3 m2) {
    return (mat3) {{
        { m1.e[0][0]*m2.e[0][0] + m1.e[1][0]*m2.e[0][1] + m1.e[2][0]*m2.e[0][2]
        , m1.e[0][1]*m2.e[0][0] + m1.e[1][1]*m2.e[0][1] + m1.e[2][1]*m2.e[0][2]
        , m1.e[0][2]*m2.e[0][0] + m1.e[1][2]*m2.e[0][1] + m1.e[2][2]*m2.e[0][2]
        },
        { m1.e[0][0]*m2.e[1][0] + m1.e[1][0]*m2.e[1][1] + m1.e[2][0]*m2.e[1][2]
        , m1.e[0][1]*m2.e[1][0] + m1.e[1][1]*m2.e[1][1] + m1.e[2][1]*m2.e[1][2]
        , m1.e[0][2]*m2.e[1][0] + m1.e[1][2]*m2.e[1][1] + m1.e[2][2]*m2.e[1][2]
        },
        { m1.e[0][0]*m2.e[2][0] + m1.e[1][0]*m2.e[2][1] + m1.e[2][0]*m2.e[2][2]
        , m1.e[0][1]*m2.e[2][0] + m1.e[1][1]*m2.e[2][1] + m1.e[2][1]*m2.e[2][2]
        , m1.e[0][2]*m2.e[2][0] + m1.e[1][2]*m2.e[2][1] + m1.e[2][2]*m2.e[2][2]
        }
    }};
}

mat3 mat3_inv(mat3 m) {
    float det = - m.e[0][2]*m.e[1][1]*m.e[2][0] + m.e[0][1]*m.e[1][2]*m.e[2][0]
                + m.e[0][2]*m.e[1][0]*m.e[2][1] - m.e[0][0]*m.e[1][2]*m.e[2][1]
                - m.e[0][1]*m.e[1][0]*m.e[2][2] + m.e[0][0]*m.e[1][1]*m.e[2][2];
    return (mat3) {{
        { (-m.e[1][2]*m.e[2][1] + m.e[1][1]*m.e[2][2]) / det
        , ( m.e[0][2]*m.e[2][1] - m.e[0][1]*m.e[2][2]) / det
        , (-m.e[0][2]*m.e[1][1] + m.e[0][1]*m.e[1][2]) / det
        },
        { ( m.e[1][2]*m.e[2][0] - m.e[1][0]*m.e[2][2]) / det
        , (-m.e[0][2]*m.e[2][0] + m.e[0][0]*m.e[2][2]) / det
        , ( m.e[0][2]*m.e[1][0] - m.e[0][0]*m.e[1][2]) / det
        },
        { (-m.e[1][1]*m.e[2][0] + m.e[1][0]*m.e[2][1]) / det
        , ( m.e[0][1]*m.e[2][0] - m.e[0][0]*m.e[2][1]) / det
        , (-m.e[0][1]*m.e[1][0] + m.e[0][0]*m.e[1][1]) / det
        }
    }};
}

mat3 mat3_sequence3(mat3 m1, mat3 m2, mat3 m3) {
    return mat3_sequence(m1, mat3_sequence(m2, m3));
}


mat3 mat3_translation(vec2 v) {
    return (mat3) {{
        { 1, 0, v.x },
        { 0, 1, v.y },
        { 0, 0,   1 }
    }};
}

mat3 mat3_rotation(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return (mat3) {{
        { c, -s, 0 },
        { s,  c, 0 },
        { 0,  0, 1 }
    }};
}

vec2 apply_homography(mat3 m, vec2 v) {
    float x = v.x * m.e[0][0] + v.y * m.e[0][1] + m.e[0][2];
    float y = v.x * m.e[1][0] + v.y * m.e[1][1] + m.e[1][2];
    float z = v.x * m.e[2][0] + v.y * m.e[2][1] + m.e[2][2];
    return (vec2){x / z, y / z};
}

mat3 rigid_motion_from_pairs(vec2 v1_pre, vec2 v1_pos,
                             vec2 v2_pre, vec2 v2_pos) {
    float angle_pre = vec2_angle(vec2_minus(v2_pre, v1_pre));
    float angle_pos = vec2_angle(vec2_minus(v2_pos, v1_pos));
    vec2 midpt_pre = vec2_lerp(v1_pre, v2_pre, 0.5);
    vec2 midpt_pos = vec2_lerp(v1_pos, v2_pos, 0.5);

    return mat3_sequence3(
        mat3_translation(vec2_unary_minus(midpt_pre)),  // Translate pre center to origin
        mat3_rotation(angle_pos - angle_pre),           // Rotate from pre seg to pos seg
        mat3_translation(midpt_pos)                     // Translate origin to pos center
    );
}
