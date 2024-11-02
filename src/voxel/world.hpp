#pragma once

#include "chunk.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "gfx/vulkan/buffer_stager.hpp"
#include "glm/gtx/hash.hpp"
#include "point_light.hpp"
#include "voxel/chunk_manager.hpp"
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
            OverwriteOnOverlap,
            // TODO: WriteInFreeSpaces
        };

        class VoxelWriteTransaction
        {
        public:

            void rollback();
            void leak();
        };
    public:
        explicit World(const game::Game*);
        ~World();

        // [[nodiscard]] Voxel              readVoxelMaterial(glm::ivec3) const;
        // [[nodiscard]] std::vector<Voxel> readVoxelMaterial(std::span<const glm::ivec3>) const;

        // [[nodiscard]] bool              readVoxelOpacity(glm::ivec3) const;
        // [[nodiscard]] std::vector<bool> readVoxelOpacity(std::span<const glm::ivec3>) const;

        void writeVoxel(WorldPosition, Voxel) const;
        // void writeVoxel(std::span<const VoxelWrite>) const;

        [[nodiscard]] VoxelWriteTransaction
             writeVoxel(VoxelWriteOverlapBehavior, std::span<const VoxelWrite>) const;
        void reverseTransaction(VoxelWriteTransaction) const;

        [[nodiscard]] PointLight
        createPointLight(glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs) const;
        void modifyPointLight(
            const PointLight&,
            glm::vec3 position,
            glm::vec4 colorAndPower,
            glm::vec4 falloffs) const;
        void destroyPointLight(PointLight) const;

        void setCamera(game::Camera) const;

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject>
        getRecordObjects(const game::Game*, const gfx::vulkan::BufferStager&);

    private:
        mutable ChunkManager                               chunk_manager;
        mutable std::unordered_map<ChunkCoordinate, Chunk> global_chunks;
        mutable game::Camera                               camera;
    };
} // namespace voxel
