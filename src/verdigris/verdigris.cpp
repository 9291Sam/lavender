
#include "verdigris.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/window.hpp"
#include "triangle_component.hpp"
#include "util/misc.hpp"
#include "util/static_filesystem.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "voxel/structures.hpp"
#include <FastNoise/FastNoise.h>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/range/numeric.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include <utility>

namespace verdigris
{
    Verdigris::Verdigris(game::Game* game_) // NOLINT
        : game {game_}
        , triangle {ecs::createEntity()}
        , chunk_render_manager {this->game}
    {
        this->triangle_pipeline = this->game->getRenderer()->getAllocator()->cachePipeline(
            gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                .stages {{
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                        .stage {vk::ShaderStageFlagBits::eVertex},
                        .shader {this->game->getRenderer()->getAllocator()->cacheShaderModule(
                            staticFilesystem::loadShader("triangle.vert"),
                            "Triangle Vertex Shader")},
                        .entry_point {"main"},
                    },
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                        .stage {vk::ShaderStageFlagBits::eFragment},
                        .shader {this->game->getRenderer()->getAllocator()->cacheShaderModule(
                            staticFilesystem::loadShader("triangle.frag"),
                            "Triangle Fragment Shader")},
                        .entry_point {"main"},
                    },
                }},
                .vertex_attributes {},
                .vertex_bindings {},
                .topology {vk::PrimitiveTopology::eTriangleList},
                .discard_enable {false},
                .polygon_mode {vk::PolygonMode::eFill},
                .cull_mode {vk::CullModeFlagBits::eNone},
                .front_face {vk::FrontFace::eClockwise},
                .depth_test_enable {true},
                .depth_write_enable {true},
                .depth_compare_op {vk::CompareOp::eLess},
                .color_format {gfx::Renderer::ColorFormat.format},
                .depth_format {gfx::Renderer::DepthFormat},
                .blend_enable {true},
                .layout {this->game->getRenderer()->getAllocator()->cachePipelineLayout(
                    gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                        .descriptors {{this->game->getGlobalInfoDescriptorSetLayout()}},
                        .push_constants {vk::PushConstantRange {
                            .stageFlags {vk::ShaderStageFlagBits::eVertex},
                            .offset {0},
                            .size {64}}},
                        .name {"Triangle Pipeline Layout"}})},
                .name {"Triangle Pipeline"}});

        this->triangle.addComponent(TriangleComponent {
            .transform {.scale {glm::vec3 {25.0f, 25.0f, 25.0f}}} // namespace verdigris
        });

        // CpuMasterBuffer
        // meshoperation (copy of everything)
        // once its done upload it to the gpu

        this->camera.addPosition({79.606, 42.586, -9.784});
        this->camera.addPitch(0.397f);
        this->camera.addYaw(3.87f);

        this->chunks.push_back(
            this->chunk_render_manager.createChunk(voxel::WorldPosition {{0, 0, 0}}));

        std::vector<voxel::ChunkLocalUpdate> updates {};

        for (int i = 0; i < 64; ++i)
        {
            for (int j = 0; j < 64; ++j)
            {
                updates.push_back(voxel::ChunkLocalUpdate {
                    voxel::ChunkLocalPosition {
                        {i,
                         static_cast<u8>(
                             (8.0 * std::sin(i / 12.0)) + (8.0 * std::cos(j / 12.0)) + 17.0),
                         j}},
                    static_cast<voxel::Voxel>((std::rand() % 11) + 1),
                    voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                    voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
            }
        }

        this->chunk_render_manager.updateChunk(this->chunks.front(), updates);

        this->chunks.push_back(
            this->chunk_render_manager.createChunk(voxel::WorldPosition {{64, 64, 64}}));
        // this->chunks.push_back(
        //     this->chunk_render_manager.createChunk(voxel::WorldPosition {{64, 0, 0}}));
    }

    Verdigris::~Verdigris()
    {
        for (voxel::ChunkRenderManager::Chunk& c : this->chunks)
        {
            this->chunk_render_manager.destroyChunk(std::move(c));
        }
    }

    game::Game::GameState::OnFrameReturnData Verdigris::onFrame(float deltaTime) const
    {
        gfx::profiler::TaskGenerator profilerTaskGenerator {};

        std::vector<voxel::ChunkLocalUpdate> us {};

        us.push_back(voxel::ChunkLocalUpdate {
            voxel::ChunkLocalPosition {{
                std::rand() % 64,
                std::rand() % 64,
                std::rand() % 64,
            }},
            static_cast<voxel::Voxel>((std::rand() % 11) + 1),
            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});

        this->chunk_render_manager.updateChunk(this->chunks.back(), us);

        std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> pDist {8.0, 16.0};

        // TODO: moving diagonally is faster
        const float moveScale        = this->game->getRenderer()->getWindow()->isActionActive(
                                    gfx::Window::Action::PlayerSprint)
                                         ? 64.0f
                                         : 36.0f;
        const float rotateSpeedScale = 6.0f;

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::CloseWindow))
        {
            this->game->terminateGame();
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ToggleCursorAttachment))
        {
            this->game->getRenderer()->getWindow()->toggleCursor();
        }

        if (this->game->getRenderer()->getFrameNumber() == 4
            || this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ResetPlayPosition))
        {
            this->camera.setPosition({79.606, 75.586, 16.78});
        }

        glm::vec3 previousPosition = this->camera.getPosition();

        glm::vec3 newPosition = previousPosition;

        // Adjust the new position based on input for movement directions
        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveForward))
        {
            newPosition += this->camera.getForwardVector() * deltaTime * moveScale;
        }
        else if (this->game->getRenderer()->getWindow()->isActionActive(
                     gfx::Window::Action::PlayerMoveBackward))
        {
            newPosition -= this->camera.getForwardVector() * deltaTime * moveScale;
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveLeft))
        {
            newPosition -= this->camera.getRightVector() * deltaTime * moveScale;
        }
        else if (this->game->getRenderer()->getWindow()->isActionActive(
                     gfx::Window::Action::PlayerMoveRight))
        {
            newPosition += this->camera.getRightVector() * deltaTime * moveScale;
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveUp))
        {
            newPosition += this->camera.getUpVector() * deltaTime * moveScale;
        }
        else if (this->game->getRenderer()->getWindow()->isActionActive(
                     gfx::Window::Action::PlayerMoveDown))
        {
            newPosition -= this->camera.getUpVector() * deltaTime * moveScale;
        }

        this->camera.setPosition(newPosition);

        auto getMouseDeltaRadians = [&]
        {
            // each value from -1.0 -> 1.0 representing how much it moved
            // on the screen
            const auto [nDeltaX, nDeltaY] =
                this->game->getRenderer()->getWindow()->getScreenSpaceMouseDelta();

            const auto deltaRadiansX = (nDeltaX / 2) * this->game->getFovXRadians();
            const auto deltaRadiansY = (nDeltaY / 2) * this->game->getFovYRadians();

            return gfx::Window::Delta {.x {deltaRadiansX}, .y {deltaRadiansY}};
        };

        auto [xDelta, yDelta] = getMouseDeltaRadians();

        this->camera.addYaw(xDelta * rotateSpeedScale);
        this->camera.addPitch(yDelta * rotateSpeedScale);

        std::vector<game::FrameGenerator::RecordObject> draws {};

        ecs::getGlobalECSManager()->iterateComponents<TriangleComponent>(
            [&](ecs::RawEntity, const TriangleComponent& c)
            {
                draws.push_back(game::FrameGenerator::RecordObject {
                    .transform {c.transform},
                    .render_pass {game::FrameGenerator::DynamicRenderingPass::SimpleColor},
                    .pipeline {this->triangle_pipeline},
                    .descriptors {
                        {this->game->getGlobalInfoDescriptorSet(), nullptr, nullptr, nullptr}},
                    .record_func {
                        [](vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, u32 id)
                        {
                            commandBuffer.pushConstants(
                                layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(u32), &id);

                            commandBuffer.draw(3, 1, 0, 0);
                        }},
                });
            });

        for (game::FrameGenerator::RecordObject& o :
             this->chunk_render_manager.processUpdatesAndGetDrawObjects(
                 this->game->getRenderer()->getStager()))
        {
            draws.push_back(std::move(o));
        }

        return OnFrameReturnData {
            .main_scene_camera {this->camera},
            .record_objects {std::move(draws)},
            .generator {profilerTaskGenerator}};
    }

    void Verdigris::onTick(float) const {}

} // namespace verdigris
