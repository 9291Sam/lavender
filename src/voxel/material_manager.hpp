#pragma once

#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "structures.hpp"
#include <glm/vec4.hpp>
#include <limits>
#include <vulkan/vulkan_enums.hpp>

namespace voxel
{

    struct VoxelMaterial
    {
        glm::vec4 ambient_color;
        glm::vec4 diffuse_color;
        glm::vec4 specular_color;
        float     diffuse_subsurface_weight;
        float     specular;
        float     roughness;
        float     metallic;
        glm::vec4 emissive_color_power;
        glm::vec4 coat_color_power;
    };

    VoxelMaterial getMaterialFromVoxel(Voxel v);

    inline gfx::vulkan::WriteOnlyBuffer<VoxelMaterial>
    generateVoxelMaterialBuffer(const gfx::Renderer* renderer)
    {
        std::vector<VoxelMaterial> materials {};
        materials.reserve(std::numeric_limits<u16>::max());

        for (u16 i = 0; i < std::numeric_limits<u16>::max(); ++i)
        {
            materials.push_back(getMaterialFromVoxel(static_cast<Voxel>(i)));
        }

        gfx::vulkan::WriteOnlyBuffer<VoxelMaterial> buffer {
            renderer->getAllocator(),
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
            materials.size(),
            "Voxel Material Buffer"};

        buffer.uploadImmediate(0, materials);

        return buffer;
    }

} // namespace voxel
