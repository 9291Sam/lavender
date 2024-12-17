#pragma once

#include "util/misc.hpp"
#include "voxel/structures.hpp"
#include <FastNoise/FastNoise.h>
#include <boost/container/detail/destroyers.hpp>
#include <random>
#include <unordered_map>

namespace world
{
    struct WorldChunkGenerator
    {
    public:
        explicit WorldChunkGenerator(std::size_t seed_) // NOLINT
            : simplex {FastNoise::New<FastNoise::Simplex>()}
            , fractal {FastNoise::New<FastNoise::FractalFBm>()}
            , seed {seed_}
        {
            this->fractal->SetSource(this->simplex);
            this->fractal->SetOctaveCount(1);
            this->fractal->SetGain(1.024f);
            this->fractal->SetLacunarity(2.534f);

            auto insertVoxelAt = [&](glm::i32vec3 p, voxel::Voxel v) mutable
            {
                this->modify_list[{voxel::WorldPosition {p}}] = v;
            };

            std::mt19937_64                    gen {std::random_device {}()};
            std::normal_distribution<float>    realDist {64, 3};
            std::uniform_int_distribution<u16> pDist {1, 8};

            this->modify_list.reserve(1024 * 1024);

            auto genVoxel = [&] -> voxel::Voxel
            {
                return static_cast<voxel::Voxel>(pDist(gen));
            };

            for (i32 x = -1024; x < 1024; ++x)
            {
                for (i32 z = -1024; z < 1024; ++z)
                {
                    insertVoxelAt(glm::ivec3 {x, 64, z}, genVoxel());
                }
            }

            for (i32 x = -256; x < 256; ++x)
            {
                for (i32 z = -256; z < 256; ++z)
                {
                    for (i32 y = 0; y < 128; ++y)
                    {
                        if (y == 0)
                        {
                            insertVoxelAt({x, y, z}, voxel::Voxel::Gold);
                        }
                    }
                }
            }

            for (i32 x = -64; x < 64; ++x)
            {
                for (i32 z = -64; z < 64; ++z)
                {
                    insertVoxelAt({x, 64, z}, voxel::Voxel::Emerald);
                }
            }

            glm::f32vec3 center = {0, 0, 0}; // Center of the structure

            // Trunk of the tree
            for (int y = 0; y < 32; ++y) // Trunk height of 10 voxels
            {
                insertVoxelAt(center + glm::f32vec3(0, y, 0), voxel::Voxel::Copper);
            }

            // Branches
            struct Branch
            {
                glm::f32vec3 direction;
                float        length;
                float        height;
            };

            // Define branch directions and lengths (4 branches)
            std::vector<Branch> branches = {
                {{3, 2, 2}, 22, 23},   // Branch 1
                {{-3, 2, -2}, 17, 17}, // Branch 2
                {{2, 2, -3}, 19, 23},  // Branch 3
                {{-2, 2, 3}, 15, 27}   // Branch 4
            };

            // Create branches and leaf clusters at their tips
            for (const Branch& branch : branches)
            {
                glm::f32vec3 start = center
                                   + glm::f32vec3(
                                         0,
                                         branch.height,
                                         0); // Start from trunk at specified height
                glm::f32vec3 direction =
                    glm::normalize(branch.direction); // Direction vector for the branch

                // Draw the branch (wood voxels)
                for (float i = 0; i < branch.length; ++i)
                {
                    glm::f32vec3 pos = start + direction * i;
                    insertVoxelAt(pos, voxel::Voxel::Obsidian);
                }

                // Leaves (spherical cluster at the tip of the branch)
                glm::f32vec3 leafCenter = start + direction * branch.length;
                float        leafRadius = 3; // Radius of the spherical leaf cluster
                for (float x = -leafRadius; x <= leafRadius; ++x)
                {
                    for (float y = -leafRadius; y <= leafRadius; ++y)
                    {
                        for (float z = -leafRadius; z <= leafRadius; ++z)
                        {
                            glm::f32vec3 leafPos = leafCenter + glm::f32vec3(x, y, z);
                            if (glm::length(glm::vec3(x, y, z))
                                <= leafRadius) // Check to make it spherical
                            {
                                insertVoxelAt(leafPos, voxel::Voxel::Copper);
                            }
                        }
                    }
                }
            }

            // // Build pillars in a circular formation
            float numPillars = 12;     // Number of pillars
            float radius     = 192.0f; // Radius of the pillar circle
            for (float i = 0; i < numPillars; ++i)
            {
                float angle = i * (2.0f * glm::pi<float>() / numPillars);

                glm::i32vec3 pillarBase = center
                                        + glm::f32vec3(
                                              static_cast<int>(radius * cos(angle)),
                                              0,
                                              static_cast<int>(radius * sin(angle)));

                for (float theta = 0; theta < 2 * glm::pi<float>(); theta += 0.06f)
                {
                    for (float r = 0; r < 16; r++)
                    {
                        for (float y = 0; y < 64; ++y)
                        {
                            insertVoxelAt(
                                pillarBase + glm::i32vec3(r * cos(theta), y, r * sin(theta)),
                                voxel::Voxel::Pearl);
                        }
                    }
                }
            }

            // // // Build doughnut ceiling with a hole in the center
            float innerRadius = 128; // Inner radius (hole size)
            float outerRadius = 240; // Outer radius (doughnut size)

            for (int h = 0; h < 24; ++h)
            {
                int currentHeight = h + 52; // Start the doughnut at a base height of 20
                for (float x = -outerRadius; x <= outerRadius; ++x)
                {
                    for (float z = -outerRadius; z <= outerRadius; ++z)
                    {
                        float dist = glm::length(glm::vec2(x, z));

                        // Check if the point is within the doughnut's ring shape
                        // (between inner and outer radius)
                        if (dist >= innerRadius && dist <= outerRadius)
                        {
                            // Add a voxel for this (x, z) position at the
                            // current
                            // height layer
                            insertVoxelAt(
                                center + glm::f32vec3(x, currentHeight, z), voxel::Voxel::Ruby);
                        }
                    }
                }
            }

            for (int x = -3; x < 4; ++x)
            {
                for (int z = -3; z < 4; ++z)
                {
                    for (int y = 0; y < 12; ++y)
                    {
                        insertVoxelAt({x, y, z}, voxel::Voxel::Ruby);
                    }
                }
            }

            for (int x = 32; x < 36; ++x)
            {
                for (int z = 32; z < 36; ++z)
                {
                    for (int y = 0; y < 32; ++y)
                    {
                        insertVoxelAt({x, y, z}, voxel::Voxel::Ruby);
                        insertVoxelAt({-x, y, z}, voxel::Voxel::Ruby);
                        insertVoxelAt({x, y, -z}, voxel::Voxel::Ruby);
                        insertVoxelAt({-x, y, -z}, voxel::Voxel::Ruby);
                    }
                }
            }

            for (int x = -64; x < 64; ++x)
            {
                for (int y = 0; y < 64; ++y)
                {
                    insertVoxelAt({x, y, -64}, voxel::Voxel::Brass);
                    insertVoxelAt({x, y, 64}, voxel::Voxel::Brass);
                    insertVoxelAt({-64, y, x}, voxel::Voxel::Brass);
                }
            }

            util::logTrace("modify list of {}", this->modify_list.size());
        }

        ~WorldChunkGenerator() = default;

        std::vector<voxel::ChunkLocalUpdate> generateChunk(voxel::WorldPosition root)
        {
            // util::logTrace(
            //     "generation of {}", glm::to_string(static_cast<glm::i32vec3>(coordinate)));

            std::vector<float> res {};
            res.resize(64UZ * 64);

            std::vector<float> mat {};
            mat.resize(64UZ * 64);

            this->fractal->GenUniformGrid2D(
                res.data(), root.z, root.x, 64, 64, 0.02f, static_cast<int>(this->seed * 13437));
            this->fractal->GenUniformGrid2D(
                mat.data(), root.z, root.x, 64, 64, 0.02f, static_cast<int>(this->seed * 8594835));
            const std::array<std::array<float, 64>, 64>* ptr = // NOLINTNEXTLINE
                reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(res.data());

            const std::array<std::array<float, 64>, 64>* matptr = // NOLINTNEXTLINE
                reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(mat.data());

            std::vector<voxel::ChunkLocalUpdate> out {};
            out.reserve(4096);

            for (std::size_t i = 0; i < 64; ++i)
            {
                for (std::size_t j = 0; j < 64; ++j)
                {
                    u8 height = static_cast<u8>(
                        std::clamp((4.0f * (*ptr)[i][j]) + 8.0f, 0.0f, 64.0f)); // NOLINT

                    for (u8 h = 0; h < 64; ++h)
                    {
                        voxel::WorldPosition worldPosition =
                            voxel::WorldPosition {{root + glm::ivec3 {i, h, j}}};

                        decltype(this->modify_list)::const_iterator maybeIt =
                            this->modify_list.find(worldPosition);

                        voxel::Voxel v = voxel::Voxel::NullAirEmpty;

                        if (maybeIt != this->modify_list.cend())
                        {
                            v = maybeIt->second;
                        }
                        else if (h < height)
                        {
                            v = static_cast<voxel::Voxel>(util::map<float>(
                                (*matptr)[i][j], -1.0f, 1.0f, 1.0f, 11.9f)); // NOLINT
                        }

                        if (v != voxel::Voxel::NullAirEmpty)
                        {
                            out.push_back(voxel::ChunkLocalUpdate {
                                voxel::ChunkLocalPosition {{i, h, j}},
                                v,
                                voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                                voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                        }
                    }
                }
            }

            return out;
        }

    private:
        FastNoise::SmartNode<FastNoise::Simplex>               simplex;
        FastNoise::SmartNode<FastNoise::FractalFBm>            fractal;
        std::size_t                                            seed;
        std::unordered_map<voxel::WorldPosition, voxel::Voxel> modify_list;
    };
} // namespace world
