
#include "verdigris.hpp"
#include "game/ec/entity_component_manager.hpp"
#include "game/game.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/window.hpp"
#include "shaders/shaders.hpp"
#include "triangle_component.hpp"
#include "voxel/chunk.hpp"
#include "voxel/chunk_manager.hpp"
#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include <glm/fwd.hpp>
#include <random>

namespace verdigris
{
    Verdigris::Verdigris(game::Game* game_)
        : game {game_}
        , ec_manager {game_->getEntityComponentManager()}
        , chunk_manager(this->game)
    {
        const game::ec::Entity entity =
            this->game->getEntityComponentManager()->createEntity();

        this->triangle_pipeline =
            this->game->getRenderer()->getAllocator()->cachePipeline(
                gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                    .stages {{
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eVertex},
                            .shader {this->game->getRenderer()
                                         ->getAllocator()
                                         ->cacheShaderModule(
                                             shaders::load("triangle.vert"))},
                            .entry_point {"main"},
                        },
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eFragment},
                            .shader {this->game->getRenderer()
                                         ->getAllocator()
                                         ->cacheShaderModule(
                                             shaders::load("triangle.frag"))},
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
                    .layout {
                        this->game->getRenderer()
                            ->getAllocator()
                            ->cachePipelineLayout(
                                gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                                    .descriptors {
                                        {this->game
                                             ->getGlobalInfoDescriptorSetLayout()}},
                                    .push_constants {vk::PushConstantRange {

                                        .stageFlags {
                                            vk::ShaderStageFlagBits::eVertex},
                                        .offset {0},
                                        .size {64}}},
                                })},
                });

        this->ec_manager->addComponent(entity, TriangleComponent {});
        // this->ec_manager->addComponent(entity, render::TriangleComponent
        // {});

        // this->ec_manager->destroyEntity(entity);

        // this->ec_manager->addComponent(
        //     entity,
        //     render::TriangleComponent {
        //         .transform.translation {glm::vec3 {10.8, 3.0, -1.4}}});
        // this->ec_manager->addComponent(
        //     entity,
        //     render::TriangleComponent {
        //         .transform.translation {glm::vec3 {-1.8, 2.1, -9.3}}});
        // this->ec_manager->addComponent(
        //     entity,
        //     render::TriangleComponent {
        //         .transform.translation {glm::vec3 {1.3, 3.0, 3.0}}});

        std::mt19937_64                    gen {std::random_device {}()};
        std::normal_distribution<float>    realDist {64, 3};
        std::uniform_int_distribution<u16> pDist {1, 15};

        auto genFunc = [](int x, int z) -> u8
        {
            return static_cast<u8>(
                       8 * std::sin(x / 24.0) + 8 * std::cos(z / 24.0) + 32.0)
                 % 64;
        };

        for (int cx = 0; cx < 1; ++cx)
        {
            for (int cz = 0; cz < 1; ++cz)
            {
                voxel::Chunk c = this->chunk_manager.allocateChunk(glm::vec3 {
                    cx * 64.0 - 32.0,
                    0.0,
                    cz * 64.0 - 32.0,
                });

                for (int i = 0; i < 64; ++i)
                {
                    for (int j = 0; j < 64; ++j)
                    {
                        this->chunk_manager.writeVoxelToChunk(
                            c,
                            voxel::ChunkLocalPosition {glm::u8vec3 {
                                static_cast<u8>(i),
                                // genFunc(i + cx * 64, j + cz * 64),
                                0,
                                static_cast<u8>(j),
                            }},
                            static_cast<voxel::Voxel>(pDist(gen)));
                    }
                }

                this->chunks.insert(std::move(c));
            }
        }
    }

    Verdigris::~Verdigris()
    {
        while (!this->chunks.empty())
        {
            std::set<voxel::Chunk>::node_type node =
                this->chunks.extract(this->chunks.begin());

            this->chunk_manager.deallocateChunk(std::move(node.value()));
        }
    }

    std::pair<game::Camera, std::vector<game::FrameGenerator::RecordObject>>
    Verdigris::onFrame(float deltaTime) const
    {
        // TODO: moving diagonally is faster
        const float moveScale =
            this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerSprint)
                ? 256.0f
                : 16.0f;
        const float rotateSpeedScale = 6.0f;

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::CloseWindow))
        {
            this->game->terminateGame();
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ToggleConsole))
        {
            util::logTrace("Frame: {} | {}", deltaTime, 1.0f / deltaTime);
        }

        this->camera.addPosition(
            this->camera.getForwardVector() * deltaTime * moveScale
            * (this->game->getRenderer()->getWindow()->isActionActive(
                   gfx::Window::Action::PlayerMoveForward)
                   ? 1.0f
                   : 0.0f));

        this->camera.addPosition(
            -this->camera.getForwardVector() * deltaTime * moveScale
            * (this->game->getRenderer()->getWindow()->isActionActive(
                   gfx::Window::Action::PlayerMoveBackward)
                   ? 1.0f
                   : 0.0f));

        this->camera.addPosition(
            -this->camera.getRightVector() * deltaTime * moveScale
            * (this->game->getRenderer()->getWindow()->isActionActive(
                   gfx::Window::Action::PlayerMoveLeft)
                   ? 1.0f
                   : 0.0f));

        this->camera.addPosition(
            this->camera.getRightVector() * deltaTime * moveScale
            * (this->game->getRenderer()->getWindow()->isActionActive(
                   gfx::Window::Action::PlayerMoveRight)
                   ? 1.0f
                   : 0.0f));

        this->camera.addPosition(
            game::Transform::UpVector * deltaTime * moveScale
            * (this->game->getRenderer()->getWindow()->isActionActive(
                   gfx::Window::Action::PlayerMoveUp)
                   ? 1.0f
                   : 0.0f));

        this->camera.addPosition(
            -game::Transform::UpVector * deltaTime * moveScale
            * (this->game->getRenderer()->getWindow()->isActionActive(
                   gfx::Window::Action::PlayerMoveDown)
                   ? 1.0f
                   : 0.0f));

        auto getMouseDeltaRadians = [&]
        {
            // each value from -1.0 -> 1.0 representing how much it moved
            // on the screen
            const auto [nDeltaX, nDeltaY] = this->game->getRenderer()
                                                ->getWindow()
                                                ->getScreenSpaceMouseDelta();

            const auto deltaRadiansX =
                (nDeltaX / 2) * this->game->getFovXRadians();
            const auto deltaRadiansY =
                (nDeltaY / 2) * this->game->getFovYRadians();

            return gfx::Window::Delta {.x {deltaRadiansX}, .y {deltaRadiansY}};
        };

        auto [xDelta, yDelta] = getMouseDeltaRadians();

        this->camera.addYaw(xDelta * rotateSpeedScale);
        this->camera.addPitch(yDelta * rotateSpeedScale);

        std::vector<game::FrameGenerator::RecordObject> draws {};

        this->game->getEntityComponentManager()
            ->iterateComponents<TriangleComponent>(
                [&](game::ec::Entity, const TriangleComponent& c)
                {
                    draws.push_back(game::FrameGenerator::RecordObject {
                        .transform {c.transform},
                        .render_pass {game::FrameGenerator::
                                          DynamicRenderingPass::SimpleColor},
                        .pipeline {this->triangle_pipeline},
                        .descriptors {
                            {this->game->getGlobalInfoDescriptorSet(),
                             nullptr,
                             nullptr,
                             nullptr}},
                        .record_func {[](vk::CommandBuffer  commandBuffer,
                                         vk::PipelineLayout layout,
                                         u32                id)
                                      {
                                          commandBuffer.pushConstants(
                                              layout,
                                              vk::ShaderStageFlagBits::eVertex,
                                              0,
                                              sizeof(u32),
                                              &id);

                                          commandBuffer.draw(3, 1, 0, 0);
                                      }},
                    });
                });

        draws.push_back(this->chunk_manager.makeRecordObject());

        return {this->camera, std::move(draws)};
    }

} // namespace verdigris
