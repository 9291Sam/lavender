#version 460

#extension GL_KHR_shader_subgroup_quad : require

#include "global_descriptor_set.glsl"
#include "types.glsl"
#include "voxel_descriptors.glsl"

layout(location = 0) out vec4 out_color;

void main()
{
    
    u32 in_face_brick_data = imageLoad(visible_voxel_image, ivec2(floor(gl_FragCoord.xy))).x;

    const u32 horz = subgroupQuadSwapHorizontal(in_face_brick_data);
    const u32 vert = subgroupQuadSwapVertical(in_face_brick_data);
    const u32 diag = subgroupQuadSwapDiagonal(in_face_brick_data);

    if (in_face_brick_data == ~0u)
    {
        // ok, we are a gap, there's a chance that this is a false gap
        if (horz != ~0u) in_face_brick_data = horz;
        else if (vert != ~0u) in_face_brick_data = vert;
        else if (diag != ~0u) in_face_brick_data = diag;
    }

    if (in_face_brick_data == ~0u)
    {
        discard;
    }
    
    const u32 brick_pointer = bitfieldExtract(in_face_brick_data, 0, 20);
    const u32 face_number   = bitfieldExtract(in_face_brick_data, 20, 9);
    const u32 normal        = bitfieldExtract(in_face_brick_data, 29, 3);
    
    const uvec3 brick_local_position = uvec3(
        face_number % 8,
        (face_number % 64) / 8,
        face_number / 64
    );

    const u32 face_id = in_face_id_bricks
            .brick[brick_pointer]
            .dir[normal]
            .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];

    out_color = vec4(in_visible_face_data.data[face_id].color.xyz, 1.0);
}