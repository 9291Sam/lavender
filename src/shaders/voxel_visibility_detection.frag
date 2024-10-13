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

    const uvec3 brick_local_position = uvec3(
        face_number % 8,
        (face_number % 64) / 8,
        face_number / 64
    );

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

        if (this_visible_face % 1024 == 0)
        {
            atomicAdd(in_number_of_visible_faces.number_of_indirect_dispatches_x, 1);

            if (this_visible_face == 0)
            {
                in_number_of_visible_faces.number_of_indirect_dispatches_y = 1;
                in_number_of_visible_faces.number_of_indirect_dispatches_z = 1;
            }
        }


        in_face_id_bricks
            .brick[brick_pointer]
            .dir[normal]
            .data[brick_local_position.x][brick_local_position.y][brick_local_position.z]
            = this_visible_face;
    }

    // // TODO: this is the race right here.
    // memoryBarrierBuffer();

    // out_face_id =  in_face_id_bricks
    //         .brick[brick_pointer]
    //         .dir[normal]
    //         .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];
}