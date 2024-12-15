#ifndef SRC_SHADERS_INCLUDE_NEW_VOXEL_DESCRIPTORS_GLSL
#define SRC_SHADERS_INCLUDE_NEW_VOXEL_DESCRIPTORS_GLSL

#include "types.glsl"

struct ChunkBrickMap
{
    u16 data[8][8][8];
};

struct PerChunkGpuData
{
    i32           world_offset_x;
    i32           world_offset_y;
    i32           world_offset_z;
    u32           brick_allocation_offset;
    ChunkBrickMap data;
};

layout(set = 1, binding = 0) readonly buffer GpuChunkDataBuffer
{
    PerChunkGpuData data[];
}
in_gpu_chunk_data;

u32 BrickMap_load(u32 chunk_id, uvec3 coord)
{
    return in_gpu_chunk_data.data[chunk_id].data.data[coord.x][coord.y][coord.z];
}

struct Voxel
{
    u16 data;
};

struct MaterialBrick
{
    Voxel data[8][8][8];
};

layout(set = 1, binding = 1) readonly buffer MaterialBrickBuffer
{
    MaterialBrick brick[];
}
in_material_bricks;

struct GreedyVoxelFace
{
    u32 data;
};
u32 GreedyVoxelFace_x(GreedyVoxelFace self)
{
    return bitfieldExtract(self.data, 0, 6);
}
u32 GreedyVoxelFace_y(GreedyVoxelFace self)
{
    return bitfieldExtract(self.data, 6, 6);
}
u32 GreedyVoxelFace_z(GreedyVoxelFace self)
{
    return bitfieldExtract(self.data, 12, 6);
}
u32 GreedyVoxelFace_width(GreedyVoxelFace self)
{
    return bitfieldExtract(self.data, 18, 6);
}
u32 GreedyVoxelFace_height(GreedyVoxelFace self)
{
    return bitfieldExtract(self.data, 24, 6);
}

layout(set = 1, binding = 2) readonly buffer GreedyVoxelFaces
{
    GreedyVoxelFace face[];
}
in_greedy_voxel_faces;

struct VoxelMaterial
{
    vec4  ambient_color;
    vec4  diffuse_color;
    vec4  specular_color;
    float diffuse_subsurface_weight;
    float specular;
    float roughness;
    float metallic;
    vec4  emissive_color_power;
    vec4  coat_color_power;
};

layout(set = 1, binding = 3) readonly buffer VoxelMaterialBuffer
{
    VoxelMaterial material[];
}
in_voxel_materials;

vec3 unpackNormalId(u32 id)
{
    const vec3 available_normals[6] = {
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, -1.0, 0.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, 0.0, 1.0),
    };

    return available_normals[id];
}

#endif // SRC_SHADERS_INCLUDE_NEW_VOXEL_DESCRIPTORS_GLSL