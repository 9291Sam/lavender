#pragma once

#include "chunk.hpp"
#include "game/frame_generator.hpp"
#include "glm/gtx/hash.hpp"
#include "point_light.hpp"
#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include <span>

namespace voxel
{

    struct WorldGenerator
    {
        WorldGenerator()          = default;
        virtual ~WorldGenerator() = default;

        [[nodiscard]] Voxel virtual generate(glm::ivec3) const = 0;
    };

    class World
    {
    public:
        struct VoxelWrite
        {
            WorldPosition position;
            Voxel         voxel;
        };

        enum class VoxelWriteOverlapBehavior
        {
            FailOnOverlap,
            OverwriteOnOverlap
        };

        class VoxelWriteTransaction
        {
        public:

            void rollback();
            void leak();
        };
    public:
        World();
        ~World();

        [[nodiscard]] Voxel              readVoxelMaterial(glm::ivec3) const;
        [[nodiscard]] std::vector<Voxel> readVoxelMaterial(std::span<const glm::ivec3>) const;

        [[nodiscard]] bool              readVoxelOpacity(glm::ivec3) const;
        [[nodiscard]] std::vector<bool> readVoxelOpacity(std::span<const glm::ivec3>) const;

        void writeVoxel(glm::ivec3, Voxel) const;
        void writeVoxel(std::span<const VoxelWrite>) const;

        [[nodiscard]] VoxelWriteTransaction
            writeVoxel(VoxelWriteOverlapBehavior, std::span<const VoxelWrite>) const;

        [[nodiscard]] PointLight
        createPointLight(glm::vec3 position, glm::vec3 colorAndPower, glm::vec4 falloffs) const;
        void destroyPointLight(PointLight) const;

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject> getRecordObjects();

    private:
        std::unordered_map<ChunkCoordinate, Chunk> global_chunks;
    };
} // namespace voxel
