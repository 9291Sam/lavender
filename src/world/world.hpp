#pragma once

#include "voxel/constants.hpp"
#include "voxel/world.hpp"
namespace world
{
    struct WorldChunkGenerator
    {
    public:
        WorldChunkGenerator(std::size_t seed) {}

        std::vector<voxel::World::VoxelWrite> generateChunk(voxel::ChunkCoordinate) {}
    };
} // namespace world