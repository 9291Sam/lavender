#pragma once

// #include "flyer.hpp"
#include "ecs/entity.hpp"
#include "game/game.hpp"
#include "laser_particle_system.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "voxel/lod_world_manager.hpp"
#include "voxel/world_manager.hpp"
#include "world/generator.hpp"
#include <vulkan/vulkan_handles.hpp>

namespace verdigris
{
    struct Verdigris : public game::Game::GameState
    {
        mutable game::Camera                             camera;
        game::Game*                                      game;
        std::shared_ptr<vk::UniquePipeline>              triangle_pipeline;
        ecs::UniqueEntity                                triangle;
        mutable float                                    absolute_scroll_y;
        mutable voxel::LodWorldManager                   lod_world_manager;
        mutable render::LaserParticleSystem              laser_particle_system;
        mutable render::LaserParticleSystem::LaserHandle laser_handle;
        // mutable voxel::ChunkRenderManager                     chunk_render_manager;
        // mutable world::WorldGenerator                         world_generator;
        // mutable std::vector<voxel::ChunkRenderManager::Chunk> chunks;

        // std::vector<voxel::ChunkRenderManager::RaytracedLight> raytraced_lights;
        // mutable voxel::WorldManager         voxel_world;
        // mutable std::vector<std::pair<glm::vec3, voxel::WorldManager::VoxelObject>> cubes;
        // mutable std::vector<Flyer>                                                  fliers;
        mutable float time_alive;

        explicit Verdigris(game::Game*);
        ~Verdigris() override;

        game::Game::GameState::OnFrameReturnData onFrame(float deltaTime) const override;

        void onTick(float deltaTime) const override;
    };
} // namespace verdigris
