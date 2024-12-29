#ifndef SRC_SHADERS_INCLUDE_COMMON_GLSL
#define SRC_SHADERS_INCLUDE_COMMON_GLSL

#ifdef __cplusplus
#define GLSL_INLINE inline
#else
#define GLSL_INLINE
#endif // __cplusplus

#ifndef __cplusplus
#include "types.glsl"
#else
#include "util/misc.hpp"
#endif // __cplusplus

GLSL_INLINE u32 gpu_hashU32(u32 x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    x *= 0xac4c1b51U;
    x ^= x >> 15;
    x *= 0x31848babU;
    x ^= x >> 14;

    return x;
}

GLSL_INLINE u32 rotate_right(u32 x, u32 r)
{
    return (x >> r) | (x << (32u - r));
}

GLSL_INLINE u32 gpu_hashCombineU32(u32 a, u32 h)
{
    a *= 0xcc9e2d51u;
    a = rotate_right(a, 17u);
    a *= 0x1b873593u;
    h ^= a;
    h = rotate_right(h, 19u);
    return h * 5u + 0xe6546b64u;
}

#endif // SRC_SHADERS_INCLUDE_COMMON_GLSL