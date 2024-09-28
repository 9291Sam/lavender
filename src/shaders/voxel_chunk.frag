#version 460

#include "types.glsl"
#include "voxel_descriptors.glsl"

layout(location = 0) in flat u32 in_chunk_id;
layout(location = 1) in vec3 in_chunk_local_position;
layout(location = 0) out vec4 out_color;

void main()
{
    
    // const vec3 available_normals[6] = {
    //     vec3(0.0, 1.0, 0.0),
    //     vec3(0.0, -1.0, 0.0),
    //     vec3(-1.0, 0.0, 0.0),
    //     vec3(1.0, 0.0, 0.0),
    //     vec3(0.0, 0.0, -1.0),
    //     vec3(0.0, 0.0, 1.0),
    // };

    // vec3 normal = available_normals[in_normal];

    const uvec3 chunk_local_position = uvec3(floor(in_chunk_local_position - 0.01));
    
    const uvec3 brick_coordinate = chunk_local_position / 8;
    const uvec3 brick_local_position = chunk_local_position % 8;

    const MaybeBrickPointer this_brick_pointer = in_brick_maps.map[in_chunk_id]
        .data[brick_coordinate.x][brick_coordinate.y][brick_coordinate.z];

    const Voxel this_voxel = in_material_bricks.brick[this_brick_pointer.pointer]
        .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];

    const vec4 this_material = in_voxel_materials.material[this_voxel.data].diffuse_color;

    out_color = vec4(this_material.xyz, 1.0);    
}
