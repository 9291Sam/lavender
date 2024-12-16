#version 460

#include "types.glsl"
#include "voxel_descriptors.glsl"

layout(location = 0) in flat u32 in_chunk_id;
layout(location = 1) in vec3 in_chunk_local_position;
layout(location = 2) in flat u32 in_normal_id;
layout(location = 3) in vec3 in_frag_location_world;

layout(location = 0) out u32 out_face_data;

void main()
{
    const uvec3 chunk_local_position = uvec3(floor(in_chunk_local_position));

    const uvec3 brick_coordinate     = chunk_local_position / 8;
    const uvec3 brick_local_position = chunk_local_position % 8;

    const u32 this_brick_pointer = BrickMap_load(in_chunk_id, brick_coordinate);

    const Voxel this_voxel =
        in_material_bricks.brick[this_brick_pointer]
            .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];

    const VoxelMaterial this_material = in_voxel_materials.material[int(this_voxel.data)];

    const u32 brick_local_number =
        brick_local_position.x + brick_local_position.y * 8 + brick_local_position.z * 64;

    u32 res = 0;

    res = bitfieldInsert(res, this_brick_pointer, 0, 20);
    res = bitfieldInsert(res, brick_local_number, 20, 9);
    res = bitfieldInsert(res, in_normal_id, 29, 3);

    out_face_data = res;
}
