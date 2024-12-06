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
        int sum = 0;

        sum += int(horz != ~0u);
        sum += int(vert != ~0u);
        sum += int(diag != ~0u);

        if (sum > 2)
        {
            // ok, we are a gap, there's a chance that this is a false gap
            if (horz != ~0u)
            {
                in_face_brick_data = horz;
            }
            else if (vert != ~0u)
            {
                in_face_brick_data = vert;
            }
            else if (diag != ~0u)
            {
                in_face_brick_data = diag;
            }
        }
    }

    const ivec2 image_size = imageSize(visible_voxel_image);
    const uvec2 center     = image_size / 2;

    bool shouldFlip = false;

    const u32 crosshairWidth = 16;
    const u32 girth          = 2;

    if (abs(floor(gl_FragCoord.x) - center.x) < girth && center.y - crosshairWidth < gl_FragCoord.y
        && gl_FragCoord.y < center.y + crosshairWidth)
    {
        shouldFlip = true;
    }
    else if (
        abs(floor(gl_FragCoord.y) - center.y) < girth && center.x - crosshairWidth < gl_FragCoord.x
        && gl_FragCoord.x < center.x + crosshairWidth)
    {
        shouldFlip = true;
    }

    // if (in_face_brick_data == ~0u && !shouldFlip)
    // {
    //     discard;
    // }

    const u32 brick_pointer = bitfieldExtract(in_face_brick_data, 0, 20);
    const u32 face_number   = bitfieldExtract(in_face_brick_data, 20, 9);
    const u32 normal        = bitfieldExtract(in_face_brick_data, 29, 3);

    const uvec3 brick_local_position =
        uvec3(face_number % 8, (face_number % 64) / 8, face_number / 64);

    const u32 face_id = face_id_map_read(in_face_brick_data);

    // in_face_id_bricks
    //         .brick[brick_pointer]
    //         .dir[normal]
    //         .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];

    out_color = vec4(
        shouldFlip ? 1.0 - in_visible_face_data.data[face_id].color.xyz
                   : in_visible_face_data.data[face_id].color.xyz,
        1.0);
}