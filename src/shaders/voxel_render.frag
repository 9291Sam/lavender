#version 460

#include "global_descriptor_set.glsl"
#include "types.glsl"
#include "voxel_descriptors.glsl"

layout(location = 0) in flat u32 in_chunk_id;
layout(location = 1) in vec3 in_chunk_local_position;
layout(location = 2) in flat u32 in_normal_id;
layout(location = 3) in vec3 in_world_position_fragment;

layout(location = 0) out u32 out_face_data;

void main()
{
    const uvec3 chunk_local_position = uvec3(floor(in_chunk_local_position));

    const uvec3 brick_coordinate     = chunk_local_position / 8;
    const uvec3 brick_local_position = chunk_local_position % 8;

    const MaybeBrickPointer this_brick_pointer =
        in_brick_maps.map[in_chunk_id]
            .data[brick_coordinate.x][brick_coordinate.y][brick_coordinate.z];

    const u32 brick_local_number =
        brick_local_position.x + brick_local_position.y * 8 + brick_local_position.z * 8 * 8;

    u32 res = 0;

    res = bitfieldInsert(res, this_brick_pointer.pointer, 0, 20);
    res = bitfieldInsert(res, brick_local_number, 20, 9);
    res = bitfieldInsert(res, in_normal_id, 29, 3);

    out_face_data = res;
}
