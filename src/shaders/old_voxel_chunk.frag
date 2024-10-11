#version 460

#include "types.glsl"
#include "voxel_descriptors.glsl"
#include "global_descriptor_set.glsl"

layout(location = 0) in flat u32 in_chunk_id;
layout(location = 1) in vec3 in_chunk_local_position;
layout(location = 2) in flat u32 in_normal_id;
layout(location = 3) in vec3 in_world_position_fragment;

layout(location = 0) out vec4 out_color;

struct PointLight {
    vec3 position;
    vec4 color_and_power;  // xyz = color, w = power
    vec4 falloffs;         // x = constant, y = linear, z = quadratic, w = cubic
};

struct CalculatedLightPower {
    vec3 diffuse_strength;
    vec3 specular_strength;
};

CalculatedLightPower calculate_light_power(
    vec3 camera_position,
    vec3 voxel_position,
    vec3 voxel_normal,
    PointLight light,
    float specular_power
) {
    vec3 to_light_vector = light.position - voxel_position;
    float to_light_distance = length(to_light_vector);
    to_light_vector /= to_light_distance;

    vec3 view_vector = normalize(camera_position - voxel_position);
    vec3 reflect_vector = 2.0 * dot(to_light_vector, voxel_normal) * voxel_normal - to_light_vector;

    float attenuation = 1.0 / (
        light.falloffs.x +
        light.falloffs.y * to_light_distance +
        light.falloffs.z * to_light_distance * to_light_distance +
        light.falloffs.w * to_light_distance * to_light_distance * to_light_distance);

    vec3 diffuse_strength = max(dot(voxel_normal, to_light_vector), 0.0) * light.color_and_power.rgb * light.color_and_power.w * attenuation;
    vec3 specular_strength = pow(max(dot(reflect_vector, view_vector), 0.0), specular_power) * light.color_and_power.rgb * light.color_and_power.w * attenuation;

    return CalculatedLightPower(diffuse_strength, specular_strength);
}

vec3 exp_strength_falloff(vec3 strengths)
{
    return 1.0 - exp2(-0.25 * strengths);
}


void main()
{
    const uvec3 chunk_local_position = uvec3(floor(in_chunk_local_position));
    
    const uvec3 brick_coordinate = chunk_local_position / 8;
    const uvec3 brick_local_position = chunk_local_position % 8;

    const MaybeBrickPointer this_brick_pointer = in_brick_maps.map[in_chunk_id]
        .data[brick_coordinate.x][brick_coordinate.y][brick_coordinate.z];

    const Voxel this_voxel = in_material_bricks.brick[this_brick_pointer.pointer]
        .data[brick_local_position.x][brick_local_position.y][brick_local_position.z];

    const VoxelMaterial this_material = in_voxel_materials.material[this_voxel.data];

    CalculatedLightPower power = calculate_light_power(
        in_global_info.camera_position.xyz,
        in_world_position_fragment.xyz,
        unpackNormalId(in_normal_id), 
        PointLight(vec3(64.0 * sin(in_global_info.time_alive), 10.0, 64.0 * cos(in_global_info.time_alive)), vec4(1.0, 1.0, 1.0, 256.0), vec4(0.0, 1.0, 0.0, 0.0)),
        1.0
    );

    vec3 realColor = clamp(
        exp_strength_falloff(power.diffuse_strength) * this_material.diffuse_color.xyz +
        exp_strength_falloff(power.specular_strength) * this_material.specular_color.xyz,
        0.0,
        1.0
    );
    
    out_color = vec4(realColor, 1.0);    
}
