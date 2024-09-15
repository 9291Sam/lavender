#version 460

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    mat4 model_view_proj;
}
in_push_constants;

void main()
{
	const vec4 positions[3] = vec4[3](
		vec4(-0.5, -0.5, 0.0, 1.0),
        vec4(0.5, -0.5, 0.0, 1.0),
        vec4(0.0, 0.5, 0.0, 1.0)
	);

    const vec4 colors[3] = vec4[3](
        vec4(1.0, 0.0, 0.0, 1.0),
        vec4(0.0, 1.0, 0.0, 1.0),
        vec4(0.0, 0.0, 1.0, 1.0)
    );

	outColor = colors[gl_VertexIndex];
    
	gl_Position = in_push_constants.model_view_proj * positions[gl_VertexIndex];
}