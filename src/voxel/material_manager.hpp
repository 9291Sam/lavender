#pragma once

#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "voxel/voxel.hpp"
#include <glm/vec4.hpp>
#include <limits>
#include <vulkan/vulkan_enums.hpp>

namespace voxel
{
    struct VoxelMaterial
    {
        glm::vec4 diffuse_color;
        glm::vec4 subsurface_color;
        glm::vec4 specular_color;
        float     diffuse_subsurface_weight;
        float     specular;
        float     roughness;
        float     metallic;
        glm::vec4 emissive_color_power;
        glm::vec4 coat_color_power;
    };

    inline VoxelMaterial getMaterialFromVoxel(Voxel v);

    inline gfx::vulkan::Buffer<VoxelMaterial>
    generateVoxelMaterialBuffer(const gfx::Renderer* renderer)
    {
        std::vector<VoxelMaterial> materials {};
        materials.reserve(std::numeric_limits<u16>::max());

        for (u16 i = 0; i < std::numeric_limits<u16>::max(); ++i)
        {
            materials.push_back(getMaterialFromVoxel(static_cast<Voxel>(i)));
        }

        gfx::vulkan::Buffer<VoxelMaterial> buffer {
            renderer->getAllocator(),
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal
                | vk::MemoryPropertyFlagBits::eHostVisible,
            materials.size()};

        std::span<VoxelMaterial> gpuMaterials = buffer.getDataNonCoherent();

        std::copy(materials.cbegin(), materials.cend(), gpuMaterials.data());
        const gfx::vulkan::FlushData flushes {
            .offset_elements {0}, .size_elements {materials.size()}};

        buffer.flush({&flushes, 1});

        return buffer;
    }

    inline VoxelMaterial getMaterialFromVoxel(Voxel v)
    {
        switch (v)
        {
        case Voxel::Stone0:
            return VoxelMaterial {.diffuse_color {0.1, 0.1, 0.1, 1.0}};
        case Voxel::Stone1:
            return VoxelMaterial {.diffuse_color {0.2, 0.2, 0.2, 1.0}};
        case Voxel::Stone2:
            return VoxelMaterial {.diffuse_color {0.3, 0.3, 0.3, 1.0}};
        case Voxel::Stone3:
            return VoxelMaterial {.diffuse_color {0.4, 0.4, 0.4, 1.0}};
        case Voxel::Stone4:
            return VoxelMaterial {.diffuse_color {0.5, 0.5, 0.5, 1.0}};
        case Voxel::Stone6:
            return VoxelMaterial {.diffuse_color {0.6, 0.6, 0.6, 1.0}};
        case Voxel::Stone7:
            return VoxelMaterial {.diffuse_color {0.7, 0.7, 0.7, 1.0}};
        case Voxel::Dirt0:
            return VoxelMaterial {.diffuse_color {0.1, 0.05, 0.0, 1.0}};
        case Voxel::Dirt1:
            return VoxelMaterial {.diffuse_color {0.2, 0.1, 0.0, 1.0}};
        case Voxel::Dirt2:
            return VoxelMaterial {.diffuse_color {0.3, 0.15, 0.0, 1.0}};
        case Voxel::Dirt3:
            return VoxelMaterial {.diffuse_color {0.4, 0.2, 0.0, 1.0}};
        case Voxel::Dirt4:
            return VoxelMaterial {.diffuse_color {0.5, 0.25, 0.0, 1.0}};
        case Voxel::Dirt5:
            return VoxelMaterial {.diffuse_color {0.6, 0.3, 0.0, 1.0}};
        case Voxel::Dirt6:
            return VoxelMaterial {.diffuse_color {0.7, 0.35, 0.0, 1.0}};
        case Voxel::Dirt7:
            return VoxelMaterial {.diffuse_color {0.8, 0.4, 0.0, 1.0}};
        case Voxel::NullAirEmpty:
            [[fallthrough]];
        default:
            return VoxelMaterial {.diffuse_color {0.8, 0.4, 0.4, 1.0}};
        }
    }
} // namespace voxel