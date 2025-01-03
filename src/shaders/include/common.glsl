#ifndef SRC_SHADERS_INCLUDE_COMMON_GLSL
#define SRC_SHADERS_INCLUDE_COMMON_GLSL

#ifdef __cplusplus

#define GLSL_INLINE inline
#include "util/misc.hpp"
#include <glm/gtx/hash.hpp>
using glm::ivec3;

#else

#define GLSL_INLINE
#include "types.glsl"

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

    return seed;
}

GLSL_INLINE u32 gpu_calculateChunkWidthUnits(u32 lod)
{
    return 64u << lod;
}

GLSL_INLINE u32 gpu_calculateChunkVoxelSizeUnits(u32 lod)
{
    if (lod == 0)
    {
        return 1;
    }
    else if (lod == 1)
    {
        return 2;
    }
    else
    {
        return 1u << lod;
    }
}

#endif // SRC_SHADERS_INCLUDE_COMMON_GLSL