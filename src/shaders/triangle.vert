#version 460

#include "global_descriptor_set.glsl"

layout(push_constant) uniform PushConstants
{
    uint matrix_id;
}
in_push_constants;

layout(location = 0) out vec4 outColor;

void main()
{
    const vec4 positions[3] =
        vec4[3](vec4(-0.5, -0.5, 0.0, 1.0), vec4(0.5, -0.5, 0.0, 1.0), vec4(0.0, 0.5, 0.0, 1.0));

    const vec4 colors[3] =
        vec4[3](vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), vec4(0.0, 0.0, 1.0, 1.0));

    outColor = colors[gl_VertexIndex];

    gl_Position = in_mvp_matrices.matrix[in_push_constants.matrix_id] * positions[gl_VertexIndex];
}