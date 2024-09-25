#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "global_descriptor_set.glsl"
#include "types.glsl"


const uvec3 TOP_FACE_POINTS[4] =
    uvec3[4](uvec3(0, 1, 0), uvec3(0, 1, 1), uvec3(1, 1, 0), uvec3(1, 1, 1));

const uvec3 BOTTOM_FACE_POINTS[4] =
    uvec3[4](uvec3(0, 0, 1), uvec3(0, 0, 0), uvec3(1, 0, 1), uvec3(1, 0, 0));

const uvec3 LEFT_FACE_POINTS[4] =
    uvec3[4](uvec3(0, 0, 1), uvec3(0, 1, 1), uvec3(0, 0, 0), uvec3(0, 1, 0));

const uvec3 RIGHT_FACE_POINTS[4] =
    uvec3[4](uvec3(1, 0, 0), uvec3(1, 1, 0), uvec3(1, 0, 1), uvec3(1, 1, 1));

const uvec3 FRONT_FACE_POINTS[4] =
    uvec3[4](uvec3(0, 0, 0), uvec3(0, 1, 0), uvec3(1, 0, 0), uvec3(1, 1, 0));

const uvec3 BACK_FACE_POINTS[4] =
    uvec3[4](uvec3(1, 0, 1), uvec3(1, 1, 1), uvec3(0, 0, 1), uvec3(0, 1, 1));

const uvec3 FACE_LOOKUP_TABLE[6][4] =
    uvec3[6][4](TOP_FACE_POINTS, BOTTOM_FACE_POINTS, LEFT_FACE_POINTS,
                RIGHT_FACE_POINTS, FRONT_FACE_POINTS, BACK_FACE_POINTS);

const u32 IDX_TO_VTX_TABLE[6] = u32[6](0, 1, 2, 2, 1, 3);

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

struct VisibiltyBrick
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
    VisibiltyBrick brick[];
} in_visibility_bricks;

layout(set = 1, binding = 3) readonly buffer GreedyVoxelFaces
{
    GreedyVoxelFace face[];
} in_greedy_voxel_faces;

layout(set = 1, binding = 4) readonly buffer VoxelMaterialBuffer
{
    VoxelMaterial material[];
} in_voxel_materials;


layout(push_constant) uniform PushConstants
{
    u32 matrix_id;
} in_push_constants;

layout(location = 0) in vec4 in_chunk_position;
layout(location = 1) in u32 in_normal;
layout(location = 2) in u32 in_chunk_id;

layout(location = 0) out vec4 out_color;

void main()
{
    const u32 face_number = gl_VertexIndex / 6;
    const u32 point_within_face = gl_VertexIndex % 6;

    const GreedyVoxelFace greedy_face = in_greedy_voxel_faces.face[face_number];

    const u32 x_pos =  GreedyVoxelFace_x(greedy_face);
    const u32 y_pos =  GreedyVoxelFace_y(greedy_face);
    const u32 z_pos =  GreedyVoxelFace_z(greedy_face);
    const u32 width =  GreedyVoxelFace_width(greedy_face);
    const u32 height = GreedyVoxelFace_height(greedy_face);

    const vec3 pos_in_chunk = vec3(float(x_pos), float(y_pos), float(z_pos));

    const vec3 face_point_local = vec3(
        FACE_LOOKUP_TABLE[in_normal][IDX_TO_VTX_TABLE[point_within_face]]);
    
    const vec3 available_normals[6] = {
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, -1.0, 0.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, 0.0, 1.0),
    };

    vec3 normal = available_normals[in_normal];

    const vec3 face_point_world =
        in_chunk_position.xyz + pos_in_chunk + face_point_local;

    gl_Position = in_mvp_matrices.matrix[in_push_constants.matrix_id] * vec4(face_point_world, 1.0);

    const uvec3 chunk_local_position = uvec3(x_pos, y_pos, z_pos);
    const uvec3 brick_coordinate = chunk_local_position / 8;
    const uvec3 brick_local_position = chunk_local_position % 8;

    const MaybeBrickPointer this_brick_pointer = in_brick_maps.map[in_chunk_id]
        .data[brick_coordinate.x][brick_coordinate.y][brick_coordinate.z];

    const Voxel this_voxel = in_material_bricks.brick[this_brick_pointer.pointer]
        .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];

    const vec4 this_material = in_voxel_materials.material[this_voxel.data].diffuse_color;

    out_color = vec4(this_material.xyz, 1.0);   
}