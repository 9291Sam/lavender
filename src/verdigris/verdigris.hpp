#pragma once

#include "ecs/entity.hpp"
#include "game/game.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "voxel/chunk.hpp"
#include "voxel/chunk_manager.hpp"
#include "voxel/world.hpp"
#include <deque>
#include <set>

namespace verdigris
{
    struct Verdigris : public game::Game::GameState
    {
        mutable game::Camera                   camera;
        game::Game*                            game;
        std::shared_ptr<vk::UniquePipeline>    triangle_pipeline;
        mutable voxel::World                   voxel_world;
        mutable std::vector<voxel::PointLight> lights;
        mutable float                          time_alive;
        mutable std::deque<float>              frame_times;
        ecs::UniqueEntity                      triangle;
        std::vector<ecs::UniqueEntity>         triangles;
        std::vector<ecs::UniqueEntity>         voxels;

        explicit Verdigris(game::Game* game_);

        ~Verdigris() override;

        std::pair<game::Camera, std::vector<game::FrameGenerator::RecordObject>>
        onFrame(float deltaTime) const override;

        void onTick(float deltaTime) const override;
    };
} // namespace verdigris
