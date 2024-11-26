#version 460

#include "global_descriptor_set.glsl"

layout(location = 0) out vec3 out_ray_direction;

void main()
{
    // const vec4 model_triangle_positions[3] = {
    //     vec4(-1.0, -1.0, 0.11, 1.0),
    //     vec4(3.0,  -1.0, 0.11, 1.0),
    //     vec4(-1.0,  3.0, 0.11, 1.0)
    // };

    // const vec3 model_position = model_triangle_positions[gl_VertexIndex].xyz;
    // const vec4 world_position_fragment_intercalc =
    //     in_global_info.model_matrix * vec4(model_position, 1.0);
    // const vec3 world_position_fragment = 
    //     world_position_fragment_intercalc.xyz /
    //     world_position_fragment_intercalc.w;


    // gl_Position = in_global_info.ptr_mvp_matrix.matrix * vec4(model_position, 1.0);
    // out_world_position = world_position_fragment;
     // Full-screen triangle vertices (NDC)
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0), // Bottom-left
        vec2(3.0, -1.0),  // Bottom-right, overshooting for full coverage
        vec2(-1.0, 3.0)   // Top-left, overshooting for full coverage
    );

    // Assign screen-space position to gl_Position
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

    // Directly calculate the ray direction in camera space
    out_ray_direction = normalize(vec3(positions[gl_VertexIndex], 1.0)); // Z=1 for forward direction
}