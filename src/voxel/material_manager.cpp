#include "material_manager.hpp"

namespace voxel
{

    VoxelMaterial getMaterialFromVoxel(Voxel v)
    {
        switch (v)
        {
        case Voxel::Emerald:
            return VoxelMaterial {
                .ambient_color {0.0215, 0.1745, 0.0215, 0.55},
                .diffuse_color {0.07568, 0.61424, 0.07568, 0.55},
                .specular_color {0.633, 0.727811, 0.633, 0.55},
                .diffuse_subsurface_weight {0.0},
                .specular {76.87483f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Ruby:
            return VoxelMaterial {
                .ambient_color {0.1745, 0.01175, 0.01175, 0.55},
                .diffuse_color {0.61424, 0.04136, 0.04136, 0.55},
                .specular_color {0.727811, 0.626959, 0.626959, 0.55},
                .diffuse_subsurface_weight {0.0},
                .specular {76.89304f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Pearl:
            return VoxelMaterial {
                .ambient_color {0.25, 0.20725, 0.20725, 0.922},
                .diffuse_color {1.0, 0.829, 0.829, 0.922},
                .specular_color {0.296648, 0.296648, 0.296648, 0.922},
                .diffuse_subsurface_weight {0.0},
                .specular {11.264f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Obsidian:
            return VoxelMaterial {
                .ambient_color {0.05375, 0.05, 0.06625, 0.82},
                .diffuse_color {0.18275, 0.17, 0.22525, 0.82},
                .specular_color {0.332741, 0.328634, 0.346435, 0.82},
                .diffuse_subsurface_weight {0.0},
                .specular {38.4394f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Brass:
            return VoxelMaterial {
                .ambient_color {0.329412, 0.223529, 0.027451, 1.0},
                .diffuse_color {0.780392, 0.568627, 0.113725, 1.0},
                .specular_color {0.992157, 0.941176, 0.807843, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {27.8974f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Chrome:
            return VoxelMaterial {
                .ambient_color {0.25, 0.25, 0.25, 1.0},
                .diffuse_color {0.4, 0.4, 0.4, 1.0},
                .specular_color {0.774597, 0.774597, 0.774597, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {76.88138f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},

            };
        case Voxel::Copper:
            return VoxelMaterial {
                .ambient_color {0.19125, 0.0735, 0.0225, 1.0},
                .diffuse_color {0.7038, 0.27048, 0.0828, 1.0},
                .specular_color {0.256777, 0.137622, 0.086014, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {12.84395f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},

            };
        case Voxel::Gold:
            return VoxelMaterial {
                .ambient_color {0.24725, 0.1995, 0.0745, 1.0},
                .diffuse_color {0.75164, 0.60648, 0.22648, 1.0},
                .specular_color {0.628281, 0.555802, 0.366065, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {51.28434f},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},

            };
        case Voxel::NullAirEmpty:
            [[fallthrough]];
        default:
            return VoxelMaterial {
                .ambient_color {0.8f, 0.4f, 0.4f, 1.0f},
                .diffuse_color {0.8f, 0.4f, 0.4f, 1.0f},
                .specular_color {0.8f, 0.4f, 0.4f, 1.0f},
                .diffuse_subsurface_weight {0.0},
                .specular {0.0},
                .roughness {0.0},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},

            };
        }
    }
} // namespace voxel
