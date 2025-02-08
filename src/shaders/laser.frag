#version 460

#include "util.glsl"

struct LaserParticleInfo
{
    f32 position_x;
    f32 position_y;
    f32 position_z;
    f32 scale;
    u32 color;
};

layout(set = 0, binding = 0) readonly buffer ParticlePositionData
{
    LaserParticleInfo particles[];
}
in_particles;

layout(push_constant) uniform Camera
{
    mat4 mvp_matrix;
    vec4 camera_forward;
    vec4 camera_right;
    vec4 camera_up;
    u32  random_seed;
}
in_push_constants;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
// layout(location = 2) in flat u32 in_particle_number;
layout(location = 4) in flat u32 in_is_visible;

layout(location = 0) out vec4 out_color;

void main()
{
    const vec2 unit_position = in_uv * 2.0 - 1.0;

    if (length(unit_position) < 1.0 && in_is_visible != 0)
    {
        out_color = in_color;
    }
    else
    {
        discard;
    }
}