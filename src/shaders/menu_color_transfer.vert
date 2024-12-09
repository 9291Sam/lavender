#version 460

#include "global_descriptor_set.glsl"
#include "types.glsl"

void main()
{
    const vec4 positions[3] =
        vec4[3](vec4(-1.0, -1.0, 0.0, 1.0), vec4(-1.0, 3.0, 0.0, 1.0), vec4(3.0, -1.0, 0.0, 1.0));

    gl_Position = positions[gl_VertexIndex];
}