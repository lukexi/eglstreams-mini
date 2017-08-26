#define SB_FOR(elem, vec, body) \
    for (int VEC_i = 0; VEC_i < sb_count(vec); VEC_i++) { \
        elem = vec[VEC_i]; \
        body \
    }

#define SB_FOR_PTR(elem_ptr, vec, body) \
    for (int VEC_i = 0; VEC_i < sb_count(vec); VEC_i++) { \
        elem_ptr = &vec[VEC_i]; \
        body \
    }

#define SB_FOR_DISTINCT_PAIR(elem1, elem2, vec, body) \
    for (int VEC_i = 0; VEC_i < sb_count(vec); VEC_i++) { \
        elem1 = vec[VEC_i]; \
        for (int VEC_j = VEC_i + 1; VEC_j < sb_count(vec); VEC_j++) { \
            elem2 = vec[VEC_j]; \
            body \
        } \
    }

#define SB_FOR_DISTINCT_PAIR_PTR(elem1_ptr, elem2_ptr, vec, body) \
    for (int VEC_i = 0; VEC_i < sb_count(vec); VEC_i++) { \
        elem1_ptr = &vec[VEC_i]; \
        for (int VEC_j = VEC_i + 1; VEC_j < sb_count(vec); VEC_j++) { \
            elem2_ptr = &vec[VEC_j]; \
            body \
        } \
    }
