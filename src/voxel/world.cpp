#include "world.hpp"
#include "constants.hpp"
#include "game/frame_generator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "voxel/chunk_manager.hpp"
#include <glm/fwd.hpp>
#include <glm/vector_relational.hpp>
#include <numbers>

namespace voxel
{
    namespace
    {
        std::unordered_map<ChunkCoordinate, Chunk>::const_iterator getOrInsertChunk(
            std::unordered_map<ChunkCoordinate, Chunk>& chunks,
            ChunkManager&                               chunkManager,
            ChunkCoordinate                             coordinate)
        {
            std::unordered_map<ChunkCoordinate, Chunk>::const_iterator maybeChunkIt =
                chunks.find(coordinate);

            if (maybeChunkIt == chunks.cend())
            {
                const auto [it, success] =
                    chunks.insert({coordinate, chunkManager.createChunk(coordinate)});

                util::assertFatal(
                    success,
                    "Duplicate insertion of chunk {}",
                    glm::to_string(static_cast<glm::i32vec3>(coordinate)));

                maybeChunkIt = it;
            }

            return maybeChunkIt;
        }
    } // namespace

    World::ChunkGenerator::~ChunkGenerator() = default;

    World::World(std::unique_ptr<ChunkGenerator> generator_, const game::Game* game)
        : generator {std::move(generator_)}
        , chunk_manager(game)
    {}

    World::~World()
    {
        while (!this->global_chunks.empty())
        {
            decltype(this->global_chunks)::node_type chunk =
                this->global_chunks.extract(this->global_chunks.begin());

            this->chunk_manager.destroyChunk(std::move(chunk.mapped()));
        }
    }

    bool World::readVoxelOpacity(WorldPosition position) const
    {
        const auto [coordinate, chunkLocalPosition] = splitWorldPosition(position);

        const decltype(this->global_chunks)::const_iterator maybeChunkIt =
            this->global_chunks.find(coordinate);

        bool isOccupied = false;

        if (maybeChunkIt != this->global_chunks.cend())
        {
            isOccupied =
                this->chunk_manager
                    .readVoxelFromChunkOpacity(maybeChunkIt->second, {&chunkLocalPosition, 1})
                    .at(0);
        }

        return isOccupied;
    }

    void World::writeVoxel(WorldPosition position, Voxel voxel) const
    {
        const auto [coordinate, chunkLocalPosition] = splitWorldPosition(position);

        decltype(this->global_chunks)::const_iterator chunkIt =
            getOrInsertChunk(this->global_chunks, this->chunk_manager, coordinate);

        const ChunkManager::VoxelWrite write {.position {chunkLocalPosition}, .voxel {voxel}};

        this->chunk_manager.writeVoxelToChunk(chunkIt->second, {&write, 1});
    }

    void World::writeVoxel(std::span<const VoxelWrite> writes) const
    {
        this->writeVoxel(VoxelWriteOverlapBehavior::OverwriteOnOverlap, writes).leak();
    }

    World::VoxelWriteTransaction World::writeVoxel(
        VoxelWriteOverlapBehavior overlapBehavior, std::span<const VoxelWrite> writes) const
    {
        std::unordered_map<ChunkCoordinate, std::vector<ChunkLocalPosition>> positionsPerChunk {};
        std::unordered_map<ChunkCoordinate, std::vector<ChunkManager::VoxelWrite>>
            writesPerChunk {};

        for (const VoxelWrite& w : writes)
        {
            const auto [coordinate, chunkLocalPosition] = splitWorldPosition(w.position);

            positionsPerChunk[coordinate].push_back(chunkLocalPosition);
            writesPerChunk[coordinate].push_back(
                ChunkManager::VoxelWrite {.position {chunkLocalPosition}, .voxel {w.voxel}});
        }

        switch (overlapBehavior)
        {
        case VoxelWriteOverlapBehavior::FailOnOverlap:
            for (const auto& [coordinate, chunkLocalPositions] : positionsPerChunk)
            {
                decltype(this->global_chunks)::const_iterator chunkIt =
                    getOrInsertChunk(this->global_chunks, this->chunk_manager, coordinate);

                if (this->chunk_manager.areAnyVoxelsOccupied(chunkIt->second, chunkLocalPositions))
                {
                    return VoxelWriteTransaction {};
                }
            }
            [[fallthrough]];
        case VoxelWriteOverlapBehavior::OverwriteOnOverlap:
            for (const auto& [coordinate, chunkWrites] : writesPerChunk)
            {
                this->chunk_manager.writeVoxelToChunk(
                    getOrInsertChunk(this->global_chunks, this->chunk_manager, coordinate)->second,
                    chunkWrites);
            }
        }

        return {std::vector<VoxelWrite> {writes.begin(), writes.end()}};
    }

    void World::setCamera(game::Camera camera_) const
    {
        this->camera = camera_;

        ChunkCoordinate cameraCoordinate =
            splitWorldPosition(WorldPosition {this->camera.getPosition()}).first;

        int radius = 5;

        for (int i = -radius; i <= radius; ++i)
        {
            for (int j = -radius; j <= radius; ++j)
            {
                ChunkCoordinate shouldExist {{cameraCoordinate.x + i, 0, cameraCoordinate.z + j}};

                if (!this->global_chunks.contains(shouldExist))
                {
                    this->generator_writes[shouldExist] = std::async(
                        [coord = shouldExist, this]
                        {
                            return this->generator->generateChunk(coord);
                        });
                }
            }
        }

        std::vector<ChunkCoordinate> toRemove {};

        for (const auto& [coordinate, chunk] : this->global_chunks)
        {
            if (glm::any(glm::greaterThan(
                    glm::abs(
                        static_cast<glm::f32vec3>(coordinate).xz()
                        - static_cast<glm::f32vec3>(cameraCoordinate).xz()),
                    glm::vec2 {static_cast<float>(radius)})))
            {
                toRemove.push_back(coordinate);
            }
        }

        std::vector<ChunkCoordinate> deq {};

        for (auto& [coord, maybeWrites] : this->generator_writes)
        {
            if (maybeWrites.valid())
            {
                this->writeVoxel(maybeWrites.get());

                deq.push_back(coord);
            }
        }

        for (ChunkCoordinate c : deq)
        {
            this->generator_writes.erase(c);
        }

        for (ChunkCoordinate c : toRemove)
        {
            decltype(this->global_chunks)::const_iterator it = this->global_chunks.find(c);

            util::assertFatal(it != this->global_chunks.cend(), "ooops");

            decltype(this->global_chunks)::node_type chunk = this->global_chunks.extract(it);

            this->chunk_manager.destroyChunk(std::move(chunk.mapped()));
        }
    }

    std::vector<game::FrameGenerator::RecordObject> World::getRecordObjects(
        const game::Game*                game,
        const gfx::vulkan::BufferStager& stager,
        gfx::profiler::TaskGenerator&    profiler)
    {
        return this->chunk_manager.makeRecordObject(game, stager, this->camera, profiler);
    }

    [[nodiscard]] PointLight
    World::createPointLight(glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs) const
    {
        PointLight p = this->chunk_manager.createPointLight();

        this->chunk_manager.modifyPointLight(p, position, colorAndPower, falloffs);

        return p;
    }

    void World::modifyPointLight(
        const PointLight& p, glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs) const
    {
        this->chunk_manager.modifyPointLight(p, position, colorAndPower, falloffs);
    }

    void World::destroyPointLight(PointLight p) const
    {
        this->chunk_manager.destroyPointLight(std::move(p));
    }
} // namespace voxel
