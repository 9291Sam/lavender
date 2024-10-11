#ifndef SRC_SHADERS_INCLUDE_VOXEL_DESCRIPTORS_GLSL
#define SRC_SHADERS_INCLUDE_VOXEL_DESCRIPTORS_GLSL

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
    vec4  diffuse_color;
    vec4  subsurface_color;
    vec4  specular_color;
    float diffuse_subsurface_weight;
    float specular;
    float roughness;
    float metallic;
    vec4  emissive_color_power;
    vec4  coat_color_power;
};

layout(set = 1, binding = 0) readonly buffer BrickMapBuffer
{
   BrickMap map[];
} in_brick_maps;

layout(set = 1, binding = 1) readonly buffer MaterialBrickBuffer
{
    MaterialBrick brick[];
} in_material_bricks;

layout(set = 1, binding = 2) readonly buffer VisilityBrickBuffer
{
    OpacityBrick brick[];
} in_opacity_bricks;

layout(set = 1, binding = 3) readonly buffer GreedyVoxelFaces
{
    GreedyVoxelFace face[];
} in_greedy_voxel_faces;

layout(set = 1, binding = 4) readonly buffer VoxelMaterialBuffer
{
    VoxelMaterial material[];
} in_voxel_materials;

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

#endif // SRC_SHADERS_INCLUDE_VOXEL_DESCRIPTORS_GLSL