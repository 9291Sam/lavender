#version 460

layout(location = 0) out vec2 ndc;

void main()
{
    vec2 joint_position_uv[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

    ndc         = joint_position_uv[gl_VertexIndex];
    gl_Position = vec4(joint_position_uv[gl_VertexIndex], 0.999999, 1.0);
}