#ifndef SRC_SHADERS_INCLUDE_VOXEL_DESCRIPTORS_GLSL
#define SRC_SHADERS_INCLUDE_VOXEL_DESCRIPTORS_GLSL

#include "types.glsl"

struct MaybeBrickPointer
{
    u32 pointer;
};

struct BrickPointer
{
    u32 pointer;
};

struct Voxel
{
    u16 data;
};

struct GreedyVoxelFace
{
    u32 data;
};
u32 GreedyVoxelFace_x(GreedyVoxelFace self) { return bitfieldExtract(self.data, 0, 6); }
u32 GreedyVoxelFace_y(GreedyVoxelFace self) { return bitfieldExtract(self.data, 6, 6); }
u32 GreedyVoxelFace_z(GreedyVoxelFace self) { return bitfieldExtract(self.data, 12, 6); }
u32 GreedyVoxelFace_width(GreedyVoxelFace self) { return bitfieldExtract(self.data, 18, 6); }
u32 GreedyVoxelFace_height(GreedyVoxelFace self) { return bitfieldExtract(self.data, 24, 6); }

struct BrickMap
{
    MaybeBrickPointer data[8][8][8];
};

struct MaterialBrick
{
    Voxel data[8][8][8];
};

struct OpacityBrick
{
    u32 data[16];
};

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

struct BrickParentInfo
{
    u32 data;
};

struct VisibilityBrick
{
    OpacityBrick view_dir[6];
};

struct VisibleFaceData
{
    u32 data;
    vec3 color;
};

struct FaceIdBrick
{
    u32 data[8][8][8];
};

struct DirectionalFaceIdBricks
{
    FaceIdBrick dir[6];
};

struct PointLight {
    vec4 position;
    vec4 color_and_power;  // xyz = color, w = power
    vec4 falloffs;         // x = constant, y = linear, z = quadratic, w = cubic
};

float estimateLightEffectiveDistance(PointLight light)
{
    return light.color_and_power.w / 256.0 * (sqrt(light.falloffs.z));
}

struct GpuChunkData
{
    ivec4 position;
};

layout(set = 1, binding = 0) readonly buffer GpuChunkDataBuffer
{
    GpuChunkData data[];
} in_chunk_data;

layout(set = 1, binding = 1) readonly buffer BrickMapBuffer
{
   BrickMap map[];
} in_brick_maps;

layout(set = 1, binding = 2) readonly buffer BrickParentInfoBuffer
{
    BrickParentInfo info[];
} in_brick_parent_info;

layout(set = 1, binding = 3) readonly buffer MaterialBrickBuffer
{
    MaterialBrick brick[];
} in_material_bricks;

layout(set = 1, binding = 4) readonly buffer OpacityBrickBuffer
{
    OpacityBrick brick[];
} in_opacity_bricks;

layout(set = 1, binding = 5) buffer VisibilityBrickBuffer
{
    VisibilityBrick brick[];
} in_visibility_bricks;

struct HashMapNode32
{
    u32 key;
    u32 value;
};

layout(set = 1, binding = 6) buffer DirectionalFaceIdBrickBuffer
{
    HashMapNode32 node[];
} in_face_id_map;

const u32 kHashTableCapacity = 1U << 24U;
const u32 kEmpty = ~0;

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
    u32 slot = integerHash(key);

    for (int i = 0; i < kHashTableCapacity; ++i)
    {
        u32 prev = atomicCompSwap(in_face_id_map.node[slot].key, kEmpty, key);

        if (prev == kEmpty || prev == key)
        {
            in_face_id_map.node[slot].value = value;
            break;
        }
        
        slot = (slot + 1) & (kHashTableCapacity-1);
    }
}

u32 face_id_map_read(u32 key)
{
    u32 slot = integerHash(key);

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
}

layout(set = 1, binding = 7) readonly buffer GreedyVoxelFaces
{
    GreedyVoxelFace face[];
} in_greedy_voxel_faces;

layout(set = 1, binding = 8) readonly buffer VoxelMaterialBuffer
{
    VoxelMaterial material[];
} in_voxel_materials;

layout(set = 1, binding = 9) buffer GlobalVoxelDataBuffer
{
    u32 number_of_visible_faces;
    u32 number_of_indirect_dispatches_x;
    u32 number_of_indirect_dispatches_y;
    u32 number_of_indirect_dispatches_z;
    u32 number_of_lights;
    u32 readback_number_of_visible_faces;
} in_global_voxel_data;

layout(set = 1, binding = 10)  buffer VisibleFaceDataBuffer
{
    VisibleFaceData data[];
} in_visible_face_data;

layout(set = 1, binding = 11) buffer PointLightsBuffer
{
    PointLight lights[];
} in_point_lights;

layout(set = 1, binding = 12) buffer GlobalChunkBuffer
{   
    u16 chunk[256][256][256];
} in_global_chunks;

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


vec3 unpackBiNormalId(u32 id)
{
    const vec3 available_normals[6] = {
        vec3(1.0, 0.0, 1.0),
        vec3(1.0, -0.0, 1.0),
        vec3(-0.0, 1.0, 1.0),
        vec3(0.0, 1.0, 1.0),
        vec3(1.0, 1.0, -0.0),
        vec3(1.0, 1.0, 0.0),
    };

    return available_normals[id];
}

#endif // SRC_SHADERS_INCLUDE_VOXEL_DESCRIPTORS_GLSL