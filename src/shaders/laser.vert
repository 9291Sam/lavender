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

const u32 IDX_TO_VTX_TABLE[6] = u32[6](0, 1, 2, 2, 1, 3);

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
// layout(location = 2) out u32 out_particle_number;
layout(location = 4) out u32 out_is_visible;

void main()
{
    const u32 particle_number   = gl_VertexIndex / 6;
    const u32 point_within_face = gl_VertexIndex % 6;

    const LaserParticleInfo thisParticleInfo = in_particles.particles[particle_number];

    const vec3 particle_center_position =
        vec3(thisParticleInfo.position_x, thisParticleInfo.position_y, thisParticleInfo.position_z);

    const vec3 r = in_push_constants.camera_right.xyz * thisParticleInfo.scale;
    const vec3 u = in_push_constants.camera_up.xyz * thisParticleInfo.scale;

    const vec3 corner_positions[4] = vec3[4](
        particle_center_position + -r + u,
        particle_center_position + -r + -u,
        particle_center_position + r + u,
        particle_center_position + r + -u);

    const vec2 uvs[4] = vec2[4](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));

    gl_Position = in_push_constants.mvp_matrix
                * vec4(corner_positions[IDX_TO_VTX_TABLE[point_within_face]], 1.0);

    const bool should_be_visible =
        bool(gpu_hashU32(in_push_constants.random_seed + particle_number) % 2);

    if (should_be_visible)
    {
        out_color = gpu_srgbToLinear(thisParticleInfo.color);
    }
    else
    {
        out_color = vec4(0.0);
    }

    out_uv         = uvs[IDX_TO_VTX_TABLE[point_within_face]];
    // out_particle_number = particle_number;
    out_is_visible = u32(should_be_visible);
}