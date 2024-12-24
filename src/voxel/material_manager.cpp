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
                .roughness {0.02f},
                .metallic {0.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Ruby:
            return VoxelMaterial {
                .ambient_color {0.1745, 0.01175, 0.01175, 0.55},
                .diffuse_color {0.61424, 0.04136, 0.04136, 0.55},
                .specular_color {0.727811, 0.626959, 0.626959, 0.55},
                .diffuse_subsurface_weight {0.0},
                .specular {76.89304f},
                .roughness {0.02f},
                .metallic {0.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Pearl:
            return VoxelMaterial {
                .ambient_color {0.25, 0.20725, 0.20725, 0.922},
                .diffuse_color {1.0, 0.829, 0.829, 0.922},
                .specular_color {0.296648, 0.296648, 0.296648, 0.922},
                .diffuse_subsurface_weight {0.0},
                .specular {11.264f},
                .roughness {0.1f},
                .metallic {0.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.00f},
            };
        case Voxel::Obsidian:
            return VoxelMaterial {
                .ambient_color {0.05375, 0.05, 0.06625, 0.82},
                .diffuse_color {0.18275, 0.17, 0.22525, 0.82},
                .specular_color {0.332741, 0.328634, 0.346435, 0.82},
                .diffuse_subsurface_weight {0.0},
                .specular {38.4394f},
                .roughness {0.2f},
                .metallic {0.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Brass:
            return VoxelMaterial {
                .ambient_color {0.329412, 0.223529, 0.027451, 1.0},
                .diffuse_color {0.780392, 0.568627, 0.113725, 1.0},
                .specular_color {0.992157, 0.941176, 0.807843, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {27.8974f},
                .roughness {0.05f},
                .metallic {0.5},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Chrome:
            return VoxelMaterial {
                .ambient_color {0.25, 0.25, 0.25, 1.0},
                .diffuse_color {0.4, 0.4, 0.4, 1.0},
                .specular_color {0.774597, 0.774597, 0.774597, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {76.88138f},
                .roughness {0.01f},
                .metallic {1.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Copper:
            return VoxelMaterial {
                .ambient_color {0.19125, 0.0735, 0.0225, 1.0},
                .diffuse_color {0.7038, 0.27048, 0.0828, 1.0},
                .specular_color {0.256777, 0.137622, 0.086014, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {12.84395f},
                .roughness {0.05f},
                .metallic {1.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Gold:
            return VoxelMaterial {
                .ambient_color {0.24725, 0.1995, 0.0745, 1.0},
                .diffuse_color {0.75164, 0.60648, 0.22648, 1.0},
                .specular_color {0.628281, 0.555802, 0.366065, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {51.28434f},
                .roughness {0.02f},
                .metallic {1.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };
        case Voxel::Topaz:
            return VoxelMaterial {
                .ambient_color {0.23125, 0.1425, 0.035, 1.0},
                .diffuse_color {0.904, 0.493, 0.058, 1.0},
                .specular_color {0.628, 0.556, 0.366, 1.0},
                .diffuse_subsurface_weight {0.1f},
                .specular {51.2f},
                .roughness {0.05f},
                .metallic {0.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };

        case Voxel::Sapphire:
            return VoxelMaterial {
                .ambient_color {0.00045, 0.00045, 0.20015, 1.0},
                .diffuse_color {0.07085, 0.23568, 0.73554, 1.0},
                .specular_color {0.71396, 0.77568, 0.78563, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {51.32f},
                .roughness {0.02f},
                .metallic {0.0},
                .emissive_color_power {0.0f},
                .coat_color_power {0.0f},
            };

        case Voxel::Amethyst:
            return VoxelMaterial {
                .ambient_color {0.17258, 0.03144, 0.34571, 1.0},
                .diffuse_color {0.58347, 0.39256, 0.79752, 1.0},
                .specular_color {0.90584, 0.81946, 0.93425, 1.0},
                .diffuse_subsurface_weight {0.1021f},
                .specular {68.00452f},
                .roughness {0.032f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Diamond:
            return VoxelMaterial {
                .ambient_color {0.25f, 0.25f, 0.25f, 1.0},
                .diffuse_color {0.9f, 0.9f, 0.9f, 1.0},
                .specular_color {0.774597f, 0.774597f, 0.774597f, 1.0},
                .diffuse_subsurface_weight {0.3f},
                .specular {76.8f},
                .roughness {0.028f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Marble:
            return VoxelMaterial {
                .ambient_color {0.19125f, 0.19125f, 0.19125f, 1.0},
                .diffuse_color {0.85f, 0.85f, 0.85f, 1.0},
                .specular_color {0.332741f, 0.332741f, 0.332741f, 1.0},
                .diffuse_subsurface_weight {0.15f},
                .specular {11.264f},
                .roughness {0.092f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Granite:
            return VoxelMaterial {
                .ambient_color {0.2175f, 0.2175f, 0.2175f, 1.0},
                .diffuse_color {0.78f, 0.73f, 0.71f, 1.0},
                .specular_color {0.245455f, 0.245455f, 0.245455f, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {9.846f},
                .roughness {0.116f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Basalt:
            return VoxelMaterial {
                .ambient_color {0.11f, 0.11f, 0.11f, 1.0},
                .diffuse_color {0.23f, 0.23f, 0.23f, 1.0},
                .specular_color {0.282723f, 0.282723f, 0.282723f, 1.0},
                .diffuse_subsurface_weight {0.0},
                .specular {8.932f},
                .roughness {0.148f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Limestone:
            return VoxelMaterial {
                .ambient_color {0.19225f, 0.19225f, 0.17225f, 1.0},
                .diffuse_color {0.78f, 0.78f, 0.74f, 1.0},
                .specular_color {0.296648f, 0.296648f, 0.296648f, 1.0},
                .diffuse_subsurface_weight {0.05f},
                .specular {10.234f},
                .roughness {0.128f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };
        case Voxel::Dirt:
            return VoxelMaterial {
                .ambient_color {0.3828125f, 0.28515625, 0.1953125f, 1.0},
                .diffuse_color {0.3828125f, 0.28515625, 0.1953125f, 1.0},
                .specular_color {0.296648f, 0.296648f, 0.296648f, 1.0},
                .diffuse_subsurface_weight {0.00f},
                .specular {0.01f},
                .roughness {1.0f},
                .metallic {0.0},
                .emissive_color_power {},
                .coat_color_power {},
            };

        case Voxel::Grass:
            return VoxelMaterial {
                .ambient_color {0.140625f, 0.28515625f, 0.12445385f, 1.0},
                .diffuse_color {0.140625f, 0.28515625f, 0.12445385f, 1.0},
                .specular_color {0.034848f, 0.05945f, 0.1642348f, 1.0},
                .diffuse_subsurface_weight {0.00f},
                .specular {0.01f},
                .roughness {1.0f},
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
