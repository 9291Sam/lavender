#ifndef SRC_SHADERS_INCLUDE_GLOBAL_DESCRIPTOR_SET_GLSL
#define SRC_SHADERS_INCLUDE_GLOBAL_DESCRIPTOR_SET_GLSL

#include "types.glsl"

layout(set = 0, binding = 0) uniform MVPBlock {
    mat4 matrix[1024];
} in_mvp_matrices;

layout(set = 0, binding = 1) uniform GlobalInfo
{
    vec4 camera_position;
    uint frame_number;
    float time_alive;
} in_global_info;

layout(set = 0, binding = 2, r32f) uniform image2D depth_buffer;
layout(set = 0, binding = 3, r32ui) uniform uimage2D visible_voxel_image;
layout(set = 0, binding = 4, r32ui) uniform uimage2D face_id_image;
layout(set = 0, binding = 5) uniform sampler do_nothing_sampler; 

#endif // SRC_SHADERS_INCLUDE_GLOBAL_DESCRIPTOR_SET_GLSL