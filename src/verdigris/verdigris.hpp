#pragma once

#include "ecs/entity.hpp"
#include "game/game.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "voxel/structures.hpp"
#include "world/generator.hpp"
#include <vulkan/vulkan_handles.hpp>

namespace verdigris
{
    struct Verdigris : public game::Game::GameState
    {
        mutable game::Camera                                       camera;
        game::Game*                                                game;
        std::shared_ptr<vk::UniquePipeline>                        triangle_pipeline;
        ecs::UniqueEntity                                          triangle;
        mutable voxel::ChunkRenderManager                          chunk_render_manager;
        std::future<std::vector<voxel::ChunkRenderManager::Chunk>> chunks;
        std::vector<voxel::ChunkRenderManager::RaytracedLight>     raytraced_lights;
        util::Mutex<std::vector<std::pair<
            const voxel::ChunkRenderManager::Chunk*,
            std::vector<voxel::ChunkLocalUpdate>>>>
            updates;

        explicit Verdigris(game::Game*);

        ~Verdigris() override;

        game::Game::GameState::OnFrameReturnData onFrame(float deltaTime) const override;

        void onTick(float deltaTime) const override;
    };
} // namespace verdigris
