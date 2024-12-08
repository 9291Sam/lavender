#pragma once

#include "chunk.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "glm/gtx/hash.hpp"
#include "point_light.hpp"
#include "voxel/chunk_manager.hpp"
#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include <concepts>
#include <future>
#include <span>
#include <unordered_map>
#include <variant>

namespace gfx::profiler
{
    struct TaskGenerator;
} // namespace gfx::profiler

namespace voxel
{
    class World
    {
    public:
        struct VoxelWrite
        {
            WorldPosition position;
            Voxel         voxel;
        };

        struct ChunkGenerator
        {
            virtual ~ChunkGenerator();
            virtual std::vector<VoxelWrite> generateChunk(ChunkCoordinate) = 0;
        };

        enum class VoxelWriteOverlapBehavior
        {
            FailOnOverlap,
            OverwriteOnOverlap,
            // TODO: WriteInFreeSpaces
        };

        class VoxelWriteTransaction
        {
        public:
            VoxelWriteTransaction() = default;
            VoxelWriteTransaction(std::vector<VoxelWrite>) {}

            void leak() {}

        private:
            // std::vector<VoxelWrite>
        };
    public:
        explicit World(std::unique_ptr<ChunkGenerator>, const game::Game*);
        ~World();

        World(const World&)                 = delete;
        World(World&&) noexcept             = default;
        World& operator= (const World&)     = delete;
        World& operator= (World&&) noexcept = default;

        [[nodiscard]] Voxel              readVoxelMaterial(WorldPosition) const;
        [[nodiscard]] std::vector<Voxel> readVoxelMaterial(std::span<const WorldPosition>) const;

        [[nodiscard]] bool              readVoxelOpacity(WorldPosition) const;
        [[nodiscard]] std::vector<bool> readVoxelOpacity(std::span<const WorldPosition>) const;

        void writeVoxel(WorldPosition, Voxel) const;
        void writeVoxel(std::span<const VoxelWrite>) const;

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

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject> getRecordObjects(
            const game::Game*, const gfx::vulkan::BufferStager&, gfx::profiler::TaskGenerator&);

    private:

        mutable std::unique_ptr<ChunkGenerator> generator;
        mutable ChunkManager                    chunk_manager;
        mutable std::unordered_map<ChunkCoordinate, std::future<std::vector<VoxelWrite>>>
                                                           generator_writes;
        mutable std::unordered_map<ChunkCoordinate, Chunk> global_chunks;
        mutable game::Camera                               camera;
    };
} // namespace voxel
