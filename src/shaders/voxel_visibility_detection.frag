#version 460

#include "global_descriptor_set.glsl"
#include "types.glsl"
#include "voxel_descriptors.glsl"

layout(location = 0) out u32 out_face_id;

void main()
{
    const u32 in_face_brick_data = imageLoad(visible_voxel_image, ivec2(floor(gl_FragCoord.xy))).x;

    if (in_face_brick_data == ~0u)
    {
        discard;
    }
    
    const u32 brick_pointer = bitfieldExtract(in_face_brick_data, 0, 20);
    const u32 face_number   = bitfieldExtract(in_face_brick_data, 20, 9);
    const u32 normal        = bitfieldExtract(in_face_brick_data, 29, 3);

    const u32 face_number_index = face_number / 32;
    const u32 face_number_bit   = face_number % 32;

    const u32 mask = (1u << face_number_bit);
    const u32 prev = atomicOr(
        in_visibility_bricks.brick[brick_pointer]
            .view_dir[normal]
            .data[face_number_index],
        mask
    );
    const bool is_first_write = (prev & mask) == 0u;

    if (is_first_write)
    {
        const u32 this_visible_face = atomicAdd(in_number_of_visible_faces.number_of_visible_faces, 1);

        in_visible_face_data.data[this_visible_face] = VisibleFaceData(
            in_face_brick_data,
            vec3(0.0)
        );
    }

    memoryBarrierBuffer();

    u32 maxelem = 0;
    if (subgroupElect())
    {
        maxelem = in_number_of_visible_faces.number_of_visible_faces;
    }

    maxelem = subgroupBroadcastFirst(maxelem);

    u32 face_id = ~0u;

    for (int i = 0; i < min(maxelem, 1024); ++i)
    {
        if (in_visible_face_data.data[i].data == in_face_brick_data)
        {
            face_id = i;

            break;
        }
    }

    if (face_id == ~0u)
    {
        face_id = 1023;
    }

    out_face_id = face_id;
}