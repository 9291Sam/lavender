#pragma once

#include "ecs/entity.hpp"
#include "game/game.hpp"
#include "skybox_manager.hpp"
#include "verdigris/flyer.hpp"
#include "voxel/world.hpp"
#include <deque>
#include <vulkan/vulkan_handles.hpp>

namespace verdigris
{
    struct Verdigris : public game::Game::GameState
    {
        mutable game::Camera                   camera;
        game::Game*                            game;
        std::shared_ptr<vk::UniquePipeline>    triangle_pipeline;
        util::Mutex<voxel::World>              voxel_world;
        mutable std::vector<voxel::PointLight> lights;
        mutable std::atomic<float>             time_alive;
        mutable std::deque<float>              frame_times;
        ecs::UniqueEntity                      triangle;
        std::vector<ecs::UniqueEntity>         triangles;
        std::vector<ecs::UniqueEntity>         voxels;
        mutable std::vector<Flyer>             flyers;
        // SkyboxManager                          skybox;

        explicit Verdigris(game::Game*);

        ~Verdigris() override;

        std::pair<game::Camera, std::vector<game::FrameGenerator::RecordObject>>
        onFrame(float deltaTime) const override;

        void onTick(float deltaTime) const override;
    };
} // namespace verdigris
