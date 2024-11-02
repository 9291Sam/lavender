#include "world.hpp"
#include "constants.hpp"
#include "game/frame_generator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include <glm/fwd.hpp>

namespace voxel
{
    World::World(const game::Game* game)
        : chunk_manager(game)
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

    void World::writeVoxel(WorldPosition position, Voxel voxel) const
    {
        const auto [coordinate, chunkLocalPosition] = splitWorldPosition(position);

        decltype(this->global_chunks)::const_iterator maybeChunkIt =
            this->global_chunks.find(coordinate);

        if (maybeChunkIt == this->global_chunks.cend())
        {
            const auto [it, success] = this->global_chunks.insert(
                {coordinate, this->chunk_manager.createChunk(coordinate)});

            util::assertFatal(
                success,
                "Duplicate insertion of chunk {}",
                glm::to_string(static_cast<glm::i32vec3>(coordinate)));

            maybeChunkIt = it;
        }

        const ChunkManager::VoxelWrite write {.position {chunkLocalPosition}, .voxel {voxel}};

        this->chunk_manager.writeVoxelToChunk(maybeChunkIt->second, {&write, 1});
    }

    void World::setCamera(game::Camera c) const
    {
        this->camera = c;
    }

    std::vector<game::FrameGenerator::RecordObject>
    World::getRecordObjects(const game::Game* game, const gfx::vulkan::BufferStager& stager)
    {
        return this->chunk_manager.makeRecordObject(game, stager, this->camera);
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
