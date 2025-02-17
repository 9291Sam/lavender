#version 460

#include "global_descriptor_set.glsl"
#include "types.glsl"
#include "voxel_descriptors.glsl"

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

const uvec3 FACE_LOOKUP_TABLE[6][4] = uvec3[6][4](
    TOP_FACE_POINTS,
    BOTTOM_FACE_POINTS,
    LEFT_FACE_POINTS,
    RIGHT_FACE_POINTS,
    FRONT_FACE_POINTS,
    BACK_FACE_POINTS);

const u32 IDX_TO_VTX_TABLE[6] = u32[6](0, 1, 2, 2, 1, 3);

layout(push_constant) uniform PushConstants
{
    u32 matrix_id;
}
in_push_constants;

layout(location = 0) in u32 in_normal_id;
layout(location = 1) in u32 in_chunk_id;

layout(location = 0) out u32 out_chunk_id;
layout(location = 1) out vec3 out_chunk_local_position;
layout(location = 2) out u32 out_normal_id;

void main()
{
    const u32 face_number       = gl_VertexIndex / 6;
    const u32 point_within_face = gl_VertexIndex % 6;

    const GreedyVoxelFace greedy_face = in_greedy_voxel_faces.face[face_number];

    const u32 x_pos  = GreedyVoxelFace_x(greedy_face);
    const u32 y_pos  = GreedyVoxelFace_y(greedy_face);
    const u32 z_pos  = GreedyVoxelFace_z(greedy_face);
    const u32 width  = GreedyVoxelFace_width(greedy_face);
    const u32 height = GreedyVoxelFace_height(greedy_face);

    const uvec3 voxel_position_in_chunk = uvec3(x_pos, y_pos, z_pos);

    const float chunk_voxel_size =
        gpu_calculateChunkVoxelSizeUnits(in_gpu_chunk_data.data[in_chunk_id].lod);

    const uvec3 face_point_local = uvec3(
        chunk_voxel_size * FACE_LOOKUP_TABLE[in_normal_id][IDX_TO_VTX_TABLE[point_within_face]]);

    vec3 scaled_face_point_local = vec3(0.0);

    const float width_scale  = float(1 + width);
    const float height_scale = float(1 + height);

    if (in_normal_id == 0 || in_normal_id == 1)
    {
        // TOP or BOTTOM face: scale X and Z
        scaled_face_point_local = vec3(
            face_point_local.x * width_scale,
            face_point_local.y,
            face_point_local.z * height_scale);
    }
    else if (in_normal_id == 2 || in_normal_id == 3)
    {
        // LEFT or RIGHT face: scale Y and Z
        scaled_face_point_local = vec3(
            face_point_local.x,
            face_point_local.y * width_scale,
            face_point_local.z * height_scale);
    }
    else
    {
        // FRONT or BACK face: scale X and Y
        scaled_face_point_local = vec3(
            face_point_local.x * width_scale,
            face_point_local.y * height_scale,
            face_point_local.z);
    }

    const vec3 point_within_chunk =
        chunk_voxel_size * voxel_position_in_chunk + scaled_face_point_local;

    const ivec3 in_chunk_position = ivec3(
        in_gpu_chunk_data.data[in_chunk_id].world_offset_x,
        in_gpu_chunk_data.data[in_chunk_id].world_offset_y,
        in_gpu_chunk_data.data[in_chunk_id].world_offset_z);

    const vec3 face_point_world = vec3(in_chunk_position.xyz) + point_within_chunk;

    const vec3 normal = unpackNormalId(in_normal_id);

    gl_Position = in_mvp_matrices.matrix[in_push_constants.matrix_id] * vec4(face_point_world, 1.0);
    out_chunk_local_position = point_within_chunk + chunk_voxel_size * (0.5 * -normal);
    // (voxel_position_in_chunk + scaled_face_point_local) + 0.5 * chunk_voxel_size
    // + chunk_voxel_size* normal;
    out_chunk_id             = in_chunk_id;
    out_normal_id            = in_normal_id;
}