
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
#include "voxel/point_light.hpp"
#include "voxel/voxel.hpp"
#include "voxel/world.hpp"
#include <boost/container_hash/hash_fwd.hpp>
#include <glm/fwd.hpp>
#include <numbers>
#include <numeric>
#include <random>
#include <utility>

//
#include <FastNoise/FastNoise.h>
//

namespace verdigris
{
    Verdigris::Verdigris(game::Game* game_) // NOLINT
        : game {game_}
        , ec_manager {game_->getEntityComponentManager()}
        , stager {this->game->getRenderer()->getAllocator()}
        , voxel_world(this->game)
    {
        this->triangle = this->game->getEntityComponentManager()->createEntity();

        this->triangle_pipeline = this->game->getRenderer()->getAllocator()->cachePipeline(
            gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                .stages {{
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                        .stage {vk::ShaderStageFlagBits::eVertex},
                        .shader {this->game->getRenderer()->getAllocator()->cacheShaderModule(
                            shaders::load("triangle.vert"), "Triangle Vertex Shader")},
                        .entry_point {"main"},
                    },
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                        .stage {vk::ShaderStageFlagBits::eFragment},
                        .shader {this->game->getRenderer()->getAllocator()->cacheShaderModule(
                            shaders::load("triangle.frag"), "Triangle Fragment Shader")},
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

        this->ec_manager->addComponent(this->triangle, TriangleComponent {});
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
        std::uniform_int_distribution<u16> pDist {1, 8};

        auto genFunc = [](i32 x, i32 z) -> i32
        {
            return static_cast<i32>(8 * std::sin(x / 24.0) + 8 * std::cos(z / 24.0) + 32.0) - 128;
        };

        auto genVoxel = [&] -> voxel::Voxel
        {
            return static_cast<voxel::Voxel>(pDist(gen));
        };

        std::vector<voxel::World::VoxelWrite> writeList {};

        auto insertVoxelAt = [&](glm::i32vec3 p, voxel::Voxel v) mutable
        {
            writeList.push_back(
                voxel::World::VoxelWrite {.position {voxel::WorldPosition {p}}, .voxel {v}});
        };

        // for (i32 x = -1024; x < 1024; ++x)
        // {
        //     for (i32 z = -1024; z < 1024; ++z)
        //     {
        //         insertVoxelAt(glm::ivec3 {x, genFunc(x, z), z}, genVoxel());
        //     }
        // }

        for (i32 x = -256; x < 256; ++x)
        {
            for (i32 z = -256; z < 256; ++z)
            {
                for (i32 y = 0; y < 128; ++y)
                {
                    if (y == 0)
                    {
                        insertVoxelAt({x, y, z}, voxel::Voxel::Gold);
                    }
                }
            }
        }

        // for (i32 x = -64; x < 64; ++x)
        // {
        //     for (i32 z = -64; z < 64; ++z)
        //     {
        //         insertVoxelAt({x, 64, z}, voxel::Voxel::Emerald);
        //     }
        // }

        glm::f32vec3 center = {0, 0, 0}; // Center of the structure

        // Trunk of the tree
        for (int y = 0; y < 32; ++y) // Trunk height of 10 voxels
        {
            insertVoxelAt(center + glm::f32vec3(0, y, 0), voxel::Voxel::Copper);
        }

        // Branches
        struct Branch
        {
            glm::f32vec3 direction;
            float        length;
            float        height;
        };

        // Define branch directions and lengths (4 branches)
        std::vector<Branch> branches = {
            {{3, 2, 2}, 22, 23},   // Branch 1
            {{-3, 2, -2}, 17, 17}, // Branch 2
            {{2, 2, -3}, 19, 23},  // Branch 3
            {{-2, 2, 3}, 15, 27}   // Branch 4
        };

        // Create branches and leaf clusters at their tips
        for (const Branch& branch : branches)
        {
            glm::f32vec3 start = center
                               + glm::f32vec3(
                                     0,
                                     branch.height,
                                     0); // Start from trunk at specified height
            glm::f32vec3 direction =
                glm::normalize(branch.direction); // Direction vector for the branch

            // Draw the branch (wood voxels)
            for (float i = 0; i < branch.length; ++i)
            {
                glm::f32vec3 pos = start + direction * i;
                insertVoxelAt(pos, voxel::Voxel::Obsidian);
            }

            // Leaves (spherical cluster at the tip of the branch)
            glm::f32vec3 leafCenter = start + direction * branch.length;
            float        leafRadius = 3; // Radius of the spherical leaf cluster
            for (float x = -leafRadius; x <= leafRadius; ++x)
            {
                for (float y = -leafRadius; y <= leafRadius; ++y)
                {
                    for (float z = -leafRadius; z <= leafRadius; ++z)
                    {
                        glm::f32vec3 leafPos = leafCenter + glm::f32vec3(x, y, z);
                        if (glm::length(glm::vec3(x, y, z))
                            <= leafRadius) // Check to make it spherical
                        {
                            insertVoxelAt(leafPos, voxel::Voxel::Copper);
                        }
                    }
                }
            }
        }

        // // Build pillars in a circular formation
        float numPillars = 12;     // Number of pillars
        float radius     = 192.0f; // Radius of the pillar circle
        for (float i = 0; i < numPillars; ++i)
        {
            float angle = i * (2.0f * glm::pi<float>() / numPillars);

            glm::i32vec3 pillarBase = center
                                    + glm::f32vec3(
                                          static_cast<int>(radius * cos(angle)),
                                          0,
                                          static_cast<int>(radius * sin(angle)));

            for (float theta = 0; theta < 2 * glm::pi<float>(); theta += 0.06f)
            {
                for (float r = 0; r < 16; r++)
                {
                    for (float y = 0; y < 64; ++y)
                    {
                        insertVoxelAt(
                            pillarBase + glm::i32vec3(r * cos(theta), y, r * sin(theta)),
                            voxel::Voxel::Pearl);
                    }
                }
            }
        }

        // // // Build doughnut ceiling with a hole in the center
        // float innerRadius = 128; // Inner radius (hole size)
        // float outerRadius = 240; // Outer radius (doughnut size)

        // for (int h = 0; h < 24; ++h)
        // {
        //     int currentHeight = h + 52; // Start the doughnut at a base height of 20
        //     for (float x = -outerRadius; x <= outerRadius; ++x)
        //     {
        //         for (float z = -outerRadius; z <= outerRadius; ++z)
        //         {
        //             float dist = glm::length(glm::vec2(x, z));

        //             // Check if the point is within the doughnut's ring shape
        //             // (between inner and outer radius)
        //             if (dist >= innerRadius && dist <= outerRadius)
        //             {
        //                 // Add a voxel for this (x, z) position at the
        //                 // current
        //                 // height layer
        //                 insertVoxelAt(
        //                     center + glm::f32vec3(x, currentHeight, z), voxel::Voxel::Ruby);
        //             }
        //         }
        //     }
        // }

        // for (int x = -3; x < 4; ++x)
        // {
        //     for (int z = -3; z < 4; ++z)
        //     {
        //         for (int y = 0; y < 12; ++y)
        //         {
        //             insertVoxelAt({x, y, z}, voxel::Voxel::Ruby);
        //         }
        //     }
        // }

        // for (int x = 32; x < 36; ++x)
        // {
        //     for (int z = 32; z < 36; ++z)
        //     {
        //         for (int y = 0; y < 32; ++y)
        //         {
        //             insertVoxelAt({x, y, z}, voxel::Voxel::Ruby);
        //             insertVoxelAt({-x, y, z}, voxel::Voxel::Ruby);
        //             insertVoxelAt({x, y, -z}, voxel::Voxel::Ruby);
        //             insertVoxelAt({-x, y, -z}, voxel::Voxel::Ruby);
        //         }
        //     }
        // }

        // for (int x = -64; x < 64; ++x)
        // {
        //     for (int y = 0; y < 64; ++y)
        //     {
        //         insertVoxelAt({x, y, -64}, voxel::Voxel::Brass);
        //         insertVoxelAt({x, y, 64}, voxel::Voxel::Brass);
        //         insertVoxelAt({-64, y, x}, voxel::Voxel::Brass);
        //     }
        // }

        auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
        FastNoise::SmartNode<FastNoise::FractalFBm> fnFractal =
            FastNoise::New<FastNoise::FractalFBm>();

        fnFractal->SetSource(fnSimplex);
        fnFractal->SetOctaveCount(5);

        std::vector<float> res {};
        res.resize(16 * 16);

        fnSimplex->GenUniformGrid2D(res.data(), 0, 0, 16, 16, 0.2f, 1337);
        int index = 0;

        for (int z = 0; z < 16; z++)
        {
            for (int y = 0; y < 16; y++)
            {
                insertVoxelAt({z, res[index++] * 23, y}, voxel::Voxel::Obsidian);
            }
        }

        // std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> ddist {-1.0, 1.0};

        auto genVec3 = [&]() -> glm::vec3
        {
            return glm::vec3 {ddist(gen), ddist(gen), ddist(gen)};
        };

        this->camera.addPosition({79.606, 15.586, 16.78});
        this->camera.addPitch(-0.12f);
        this->camera.addYaw(4.87f);

        for (int i = 0; i < 2; ++i)
        {
            this->lights.push_back(this->voxel_world.createPointLight({}, {}, {}));
        }

        std::ignore = this->voxel_world.writeVoxel(
            voxel::World::VoxelWriteOverlapBehavior::FailOnOverlap, writeList);
    }

    Verdigris::~Verdigris()
    {
        for (voxel::PointLight& l : this->lights)
        {
            this->voxel_world.destroyPointLight(std::move(l));
        }
    }

    std::pair<game::Camera, std::vector<game::FrameGenerator::RecordObject>>
    Verdigris::onFrame(float deltaTime) const
    {
        this->frame_times.push_back(deltaTime);

        if (this->frame_times.size() > 128)
        {
            this->frame_times.pop_front();
        }

        const float averageFrameTime =
            std::accumulate(this->frame_times.cbegin(), this->frame_times.cend(), 0.0)
            / this->frame_times.size();

        this->time_alive += deltaTime;
        std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> pDist {-1.0, 1.0};

        auto genVec3 = [&]() -> glm::vec3
        {
            return glm::vec3 {pDist(gen), pDist(gen), pDist(gen)};
        };

        auto genSpiralPos = [](i32 f)
        {
            const float t = static_cast<float>(f) / 256.0f;

            const float x = 32.0f * std::sin(t);
            const float z = 32.0f * std::cos(t);

            return glm::i32vec3 {static_cast<i32>(x), 66.0, static_cast<i32>(z)};
        };

        const i32 frameNumber = static_cast<i32>(this->game->getRenderer()->getFrameNumber());

        // util::logTrace("modify light");
        for (int i = 0; i < 2; i++)
        {
            const float offset = this->time_alive * 2 + i * 2 * std::numbers::pi_v<float> / 8.0;

            if (i == 0)
            {
                const glm::vec3 pos = glm::vec3 {
                    28.0f * std::cos(offset),
                    4.0f * std::sin(offset) + 48.0f,
                    28.0f * std::sin(offset)};

                // util::logTrace("modify light2");

                this->voxel_world.modifyPointLight(
                    this->lights[i], pos, {1.0, 1.0, 1.0, 512.0}, {0.0, 0.0, 0.025, 0.0});

                this->ec_manager->modifyComponent<TriangleComponent>(
                    this->triangle,
                    [&](TriangleComponent& t)
                    {
                        t.transform.translation = pos;
                        t.transform.scale       = glm::vec3 {4.0f};
                    });
            }
            else
            {
                const glm::vec3 pos = glm::vec3 {
                    256.0f * std::cos(offset * 1.384 + 93.4),
                    4.0f * std::sin(offset * 1.384 + 93.4) + 8.0f,
                    256.0f * std::sin(offset * 1.384 + 93.4)};

                this->voxel_world.modifyPointLight(
                    this->lights[i], pos, {1.0, 1.0, 1.0, 2048.0}, {0.0, 0.0, 0.025, 0.0});
            }
        }

        auto thisPos = genSpiralPos(frameNumber);
        auto prevPos = genSpiralPos(frameNumber - 1);

        // if (thisPos != prevPos)
        // {
        //     for (i32 i = 0; i < 32; ++i)
        //     {
        //         this->chunk_manager.writeVoxel(
        //             thisPos + glm::i32vec3 {0, i, 0}, voxel::Voxel::Pearl);
        //         this->chunk_manager.writeVoxel(
        //             prevPos + glm::i32vec3 {0, i, 0},
        //             voxel::Voxel::NullAirEmpty);
        //     }
        // }

        // TODO: moving diagonally is faster
        const float moveScale        = this->game->getRenderer()->getWindow()->isActionActive(
                                    gfx::Window::Action::PlayerSprint)
                                         ? 64.0f
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
            util::logTrace(
                "Frame: {} | {} | {} | {}",
                deltaTime,
                1.0f / deltaTime,
                1.0f / averageFrameTime,
                static_cast<std::string>(this->camera));
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ToggleCursorAttachment))
        {
            this->game->getRenderer()->getWindow()->toggleCursor();
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

        this->game->getEntityComponentManager()->iterateComponents<TriangleComponent>(
            [&](game::ec::Entity, const TriangleComponent& c)
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

        draws.push_back(game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::PreFrameUpdate},
            .pipeline {},
            .descriptors {},
            .record_func {[this](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              this->stager.flushTransfers(commandBuffer);
                          }},
        });

        this->voxel_world.setCamera(this->camera);

        draws.append_range(this->voxel_world.getRecordObjects(this->game, this->stager));

        return {this->camera, std::move(draws)};
    }

} // namespace verdigris
