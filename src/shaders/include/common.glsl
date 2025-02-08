#ifndef SRC_SHADERS_INCLUDE_COMMON_GLSL
#define SRC_SHADERS_INCLUDE_COMMON_GLSL

#ifdef __cplusplus

#define GLSL_INLINE inline
#include "util/misc.hpp"
#include <glm/gtx/hash.hpp>
using glm::ivec3;
using glm::vec3;
using glm::vec4;

#else

#define GLSL_INLINE
#include "types.glsl"

#endif // __cplusplus

// https://nullprogram.com/blog/2018/07/31/
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

struct Gpu_ChunkLocation
{
    ivec3 root_position;
    u32   lod;
};

GLSL_INLINE u32 gpu_hashChunkCoordinate(Gpu_ChunkLocation location)
{
    u32 seed = 0xE727328CU;

    seed = gpu_hashCombineU32(seed, gpu_hashU32(u32(location.root_position.x)));

    seed = gpu_hashCombineU32(seed, gpu_hashU32(u32(location.root_position.y)));

    seed = gpu_hashCombineU32(seed, gpu_hashU32(u32(location.root_position.z)));

    seed = gpu_hashCombineU32(seed, gpu_hashU32(location.lod));

    return seed;
}

GLSL_INLINE u32 gpu_calculateChunkWidthUnits(u32 lod)
{
    return 64u << lod;
}

GLSL_INLINE u32 gpu_calculateChunkVoxelSizeUnits(u32 lod)
{
    return 1u << lod;
}

GLSL_INLINE u32 gpu_linearToSRGB(vec4 color)
{
    vec3 t;
    t.x = pow(color.x, 1.0f / 2.2f);
    t.y = pow(color.y, 1.0f / 2.2f);
    t.z = pow(color.z, 1.0f / 2.2f);

    u32 result = 0;
    result |= (0xFF & u32(t.x * 255.0f));
    result |= ((0xFF & u32(t.y * 255.0f)) << 8);
    result |= ((0xFF & u32(t.z * 255.0f)) << 16);
    result |= ((0xFF & u32(color.w * 255.0f)) << 24);

    return result;
}

GLSL_INLINE vec4 gpu_srgbToLinear(u32 srgb)
{
    vec4 result;
    result.x = f32(srgb & 0xFF) / 255.0f;
    result.y = f32((srgb >> 8) & 0xFF) / 255.0f;
    result.z = f32((srgb >> 16) & 0xFF) / 255.0f;

    result.x = pow(result.x, 2.2f);
    result.y = pow(result.y, 2.2f);
    result.z = pow(result.z, 2.2f);

    result.w = f32((srgb >> 24) & 0xFF) / 255.0f;

    return result;
}

#endif // SRC_SHADERS_INCLUDE_COMMON_GLSL