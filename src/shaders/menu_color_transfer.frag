#version 460

#include "global_descriptor_set.glsl"
#include "types.glsl"

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 color = imageLoad(menu_transfer_image, ivec2(floor(gl_FragCoord.xy)));

    color.rgb = pow(color.rgb, vec3(2.2));

    out_color = color;
}