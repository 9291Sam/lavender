#version 460

#include "global_descriptor_set.glsl"

layout(push_constant) uniform PushConstants
{
    mat4 model_matrix;
    vec4 camera_position;
    vec4 camera_normal;
    uint mvp_matricies_id;
    uint draw_id;
    float time_alive;
} in_push_constants;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_camera_position;

void main()
{
    const vec4 modelspace_triangle_positions[3] = {
        vec4(-1.0, -1.0, -0.999, 1.0),
        vec4(3.0,  -1.0, -0.999, 1.0),
        vec4(-1.0,  3.0, -0.999, 1.0)
    };

    const vec3 modelspace_position = modelspace_triangle_positions[gl_VertexIndex].xyz;
    const vec4 world_position_fragment_intercalc = in_push_constants.model_matrix * vec4(modelspace_position, 1.0);
    const vec3 world_position_fragment = world_position_fragment_intercalc.xyz / world_position_fragment_intercalc.w;


    gl_Position =  vec4(modelspace_position * vec3(1.0, 1.0, -1.0), 1.0);
    out_world_position =  world_position_fragment;
}