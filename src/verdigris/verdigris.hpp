#pragma once

#include "ecs/entity.hpp"
#include "game/game.hpp"
#include "voxel/world_manager.hpp"
#include <vulkan/vulkan_handles.hpp>

namespace verdigris
{
    struct Verdigris : public game::Game::GameState
    {
        mutable game::Camera                camera;
        game::Game*                         game;
        std::shared_ptr<vk::UniquePipeline> triangle_pipeline;
        ecs::UniqueEntity                   triangle;
        mutable voxel::WorldManager         voxel_world;

        explicit Verdigris(game::Game*);
        ~Verdigris() override;

        game::Game::GameState::OnFrameReturnData onFrame(float deltaTime) const override;

        void onTick(float deltaTime) const override;
    };
} // namespace verdigris
