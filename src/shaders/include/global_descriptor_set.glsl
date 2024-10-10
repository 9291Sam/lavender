#ifndef SRC_SHADERS_INCLUDE_GLOBAL_DESCRIPTOR_SET_GLSL
#define SRC_SHADERS_INCLUDE_GLOBAL_DESCRIPTOR_SET_GLSL

layout(set = 0, binding = 0) uniform MVPBlock {
    mat4 matrix[1024];
} in_mvp_matrices;

layout(set = 0, binding = 1) uniform GlobalInfo
{
    vec4 camera_position;
    uint frame_number;
    float time_alive;
} in_global_info;

#endif // SRC_SHADERS_INCLUDE_GLOBAL_DESCRIPTOR_SET_GLSL