#ifndef SRC_SHADERS_INCLUDE_NEW_VOXEL_DESCRIPTORS_GLSL
#define SRC_SHADERS_INCLUDE_NEW_VOXEL_DESCRIPTORS_GLSL

#include "common.glsl"
#include "types.glsl"

layout(set = 1, binding = 0) buffer GlobalVoxelDataBuffer
{
    u32 number_of_visible_faces;
    u32 number_of_indirect_dispatches_x;
    u32 number_of_indirect_dispatches_y;
    u32 number_of_indirect_dispatches_z;
    u32 number_of_lights;
    u32 readback_number_of_visible_faces;
}
in_global_voxel_data;

struct GpuRaytracedLight
{
    vec4 position_and_half_intensity_distance;
    vec4 color_and_power;
};

layout(set = 1, binding = 1) readonly buffer RaytracedLightDataBuffer
{
    GpuRaytracedLight data[];
}
in_raytraced_lights;

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

layout(set = 1, binding = 2) readonly buffer GpuChunkDataBuffer
{
    PerChunkGpuData data[];
}
in_gpu_chunk_data;

u32 calculateHashOfChunkCoordinate(i32vec3 c)
{
    u32 seed = 0xE727328CU;

    seed = gpu_hashCombineU32(seed, gpu_hashU32(u32(c.x)));

    seed = gpu_hashCombineU32(seed, gpu_hashU32(u32(c.y)));

    seed = gpu_hashCombineU32(seed, gpu_hashU32(u32(c.z)));

    return seed;
}

layout(set = 1, binding = 3) readonly buffer AlignedChunkHashTableKeys
{
    u32 keys[];
}
in_aligned_chunk_hash_table_keys;

layout(set = 1, binding = 4) readonly buffer AlignedChunkHashTableValues
{
    u16 values[];
}
in_aligned_chunk_hash_table_values;

const u32 MaxChunkHashNodes = 1U << 16U;
const u32 ChunkHashEmpty    = ~0u;

u16 ChunkHashTable_load(i32vec3 chunkCoordinate)
{
    const u32 key = calculateHashOfChunkCoordinate(chunkCoordinate);

    uint32_t slot = key % MaxChunkHashNodes;

    while (true)
    {
        if (in_aligned_chunk_hash_table_keys.keys[slot] == key)
        {
            return in_aligned_chunk_hash_table_values.values[slot];
        }
        if (in_aligned_chunk_hash_table_keys.keys[slot] == ChunkHashEmpty)
        {
            return u16(~0u);
        }
        slot = (slot + 1) % MaxChunkHashNodes;
    }
}

u32 BrickMap_load(u32 chunk_id, uvec3 coord)
{
    const u16 maybeOffset = in_gpu_chunk_data.data[chunk_id].data.data[coord.x][coord.y][coord.z];

    if (maybeOffset == u16(-1))
    {
        return ~0u;
    }
    else
    {
        return maybeOffset + in_gpu_chunk_data.data[chunk_id].brick_allocation_offset;
    }
}

struct BrickParentInfo
{
    u32 data;
};

layout(set = 1, binding = 5) readonly buffer BrickParentInfoBuffer
{
    BrickParentInfo info[];
}
in_brick_parent_info;

struct Voxel
{
    u16 data;
};

struct MaterialBrick
{
    Voxel data[8][8][8];
};

layout(set = 1, binding = 6) readonly buffer MaterialBrickBuffer
{
    MaterialBrick brick[];
}
in_material_bricks;

struct BitBrick
{
    u32 data[16];
};

layout(set = 1, binding = 7) readonly buffer ShadowBrickBuffer
{
    BitBrick brick[];
}
in_shadow_bricks;

struct VisibilityBrick
{
    BitBrick view_dir[6];
};

layout(set = 1, binding = 8) buffer VisibilityBrickBuffer
{
    VisibilityBrick brick[];
}
in_visibility_bricks;

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

layout(set = 1, binding = 9) readonly buffer GreedyVoxelFaces
{
    GreedyVoxelFace face[];
}
in_greedy_voxel_faces;

struct HashMapNode32
{
    u32 key;
    u32 value;
};

layout(set = 1, binding = 10) buffer VisibleFaceIdBrickStorage
{
    HashMapNode32 node[];
}
in_face_id_map;

const u32 kHashTableCapacity = 1U << 23U;
const u32 kEmpty             = ~0;

u32 integerHash(u32 h)
{
    h ^= h >> 17;
    h *= 0xed5ad4bbU;
    h ^= h >> 11;
    h *= 0xac4c1b51U;
    h ^= h >> 15;
    h *= 0x31848babU;
    h ^= h >> 14;

    return h;
}

void face_id_map_write(u32 key, u32 value)
{
    u32 slot = integerHash(key) & (kHashTableCapacity - 1);

    for (int i = 0; i < kHashTableCapacity; ++i)
    {
        u32 prev = atomicCompSwap(in_face_id_map.node[slot].key, kEmpty, key);

        if (prev == kEmpty || prev == key)
        {
            in_face_id_map.node[slot].value = value;
            break;
        }

        slot = (slot + 1) & (kHashTableCapacity - 1);
    }
}

u32 face_id_map_read(u32 key)
{
    u32 slot = integerHash(key) & (kHashTableCapacity - 1);

    for (int i = 0; i < kHashTableCapacity; ++i)
    {
        if (in_face_id_map.node[slot].key == key)
        {
            return in_face_id_map.node[slot].value;
        }
        if (in_face_id_map.node[slot].key == kEmpty)
        {
            return kEmpty;
        }
        slot = (slot + 1) & (kHashTableCapacity - 1);
    }

    return kEmpty;
}

struct VisibleFaceData
{
    u32  data;
    vec3 color;
};

layout(set = 1, binding = 11) buffer VisibleFaceDataBuffer
{
    VisibleFaceData data[];
}
in_visible_face_data;

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

layout(set = 1, binding = 12) readonly buffer VoxelMaterialBuffer
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