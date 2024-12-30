#ifndef SRC_SHADERS_INCLUDE_UTIL_GLSL
#define SRC_SHADERS_INCLUDE_UTIL_GLSL

#include "common.glsl"

i32 moduloEuclideani32(i32 lhs, i32 rhs)
{
    return (lhs % rhs + rhs) % rhs;
}

i32 divideEuclideani32(i32 lhs, i32 rhs)
{
    int quotient  = lhs / rhs;
    int remainder = lhs % rhs;

    // Adjust quotient for Euclidean behavior
    if (remainder != 0 && ((rhs < 0) != (lhs < 0)))
    {
        quotient -= 1;
    }

    return quotient;
}

float rand(vec3 pos)
{
    u32 seed = 474838454;

    seed = gpu_hashCombineU32(seed, gpu_hashU32(floatBitsToUint(pos.x)));

    seed = gpu_hashCombineU32(seed, gpu_hashU32(floatBitsToUint(pos.y)));

    seed = gpu_hashCombineU32(gpu_hashU32(floatBitsToUint(pos.z)), seed);

    return float(seed) / float(u32(-1));
}

float map(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#endif // SRC_SHADERS_INCLUDE_UTIL_GLSL