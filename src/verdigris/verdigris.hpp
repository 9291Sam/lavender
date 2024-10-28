#pragma once

#include "game/game.hpp"
#include "gfx/vulkan/buffer_stager.hpp"
#include "voxel/chunk.hpp"
#include "voxel/chunk_manager.hpp"
#include <deque>
#include <set>

namespace verdigris
{
    struct Verdigris : public game::Game::GameState
    {
        mutable game::Camera                    camera;
        game::Game*                             game;
        const game::ec::EntityComponentManager* ec_manager;
        std::shared_ptr<vk::UniquePipeline>     triangle_pipeline;
        gfx::vulkan::BufferStager               stager;
        mutable voxel::ChunkManager             chunk_manager;
        mutable std::vector<voxel::PointLight>  lights;
        mutable float                           time_alive;
        mutable std::deque<float>               frame_times;

        explicit Verdigris(game::Game* game_);

        ~Verdigris() override;

        std::pair<game::Camera, std::vector<game::FrameGenerator::RecordObject>>
        onFrame(float deltaTime) const override;

        void onTick() const override {}
    };
} // namespace verdigris
