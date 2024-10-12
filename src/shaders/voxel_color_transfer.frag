#version 460

#include "global_descriptor_set.glsl"
#include "types.glsl"
#include "voxel_descriptors.glsl"

layout(location = 0) out vec4 out_color;

void main()
{
    const u32 face_id = imageLoad(face_id_image, ivec2(floor(gl_FragCoord.xy))).x;

    out_color = vec4(in_visible_face_data.data[face_id].color.xyz, 1.0);
}