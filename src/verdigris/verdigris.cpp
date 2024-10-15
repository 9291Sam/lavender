
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
#include "voxel/voxel.hpp"
#include <boost/container_hash/hash_fwd.hpp>
#include <functional>
#include <glm/fwd.hpp>
#include <iterator>
#include <random>

namespace verdigris
{
    Verdigris::Verdigris(game::Game* game_) // NOLINT
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
                    .blend_enable {true},
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

        // auto genFunc = [](int x, int z) -> u8
        // {
        //     return static_cast<u8>(
        //                8 * std::sin(x / 24.0) + 8 * std::cos(z / 24.0)
        //                + 32.0)
        //          % 64;
        // };

        auto genVoxel = [&] -> voxel::Voxel
        {
            return static_cast<voxel::Voxel>(pDist(gen));
        };

        struct Bar
        {
            std::size_t operator() (glm::i32vec3 p) const
            {
                std::size_t foo {48584};

                boost::hash_combine(foo, std::hash<i32> {}(p.x));
                boost::hash_combine(foo, std::hash<i32> {}(p.y));
                boost::hash_combine(foo, std::hash<i32> {}(p.z));

                return foo;
            }
        };

        std::unordered_map<glm::i32vec3, voxel::Chunk, Bar> chunks {};

        auto eucRem = [](i32 a, i32 b)
        {
            i32 r = a % b;
            return r >= 0 ? r : r + std::abs(b);
        };

        auto insertVoxelAt = [&](glm::i32vec3 p, voxel::Voxel v)
        {
            // glm::i32vec3 base = {
            //     64 * ((p.x < 0 ? (p.x + 1) / 64 - 1 : p.x / 64)),
            //     64 * ((p.y < 0 ? (p.y + 1) / 64 - 1 : p.y / 64)),
            //     64 * ((p.z < 0 ? (p.z + 1) / 64 - 1 : p.z / 64)),
            // };

            // // util::logTrace("{} {}", glm::to_string(base),
            // glm::to_string(p));

            // if (!chunks.contains(base))
            // {
            //     chunks[base] = this->chunk_manager.allocateChunk(base);
            // }

            // this->chunk_manager.writeVoxelToChunk(
            //     chunks[base], voxel::ChunkLocalPosition {p - base},
            //     genVoxel());

            this->chunk_manager.writeVoxel(p, v);
        };

        // for (i32 x = 0; x < 64; ++x)
        // {
        //     for (i32 z = 0; z < 64; ++z)
        //     {
        //         insertVoxelAt(glm::ivec3 {x, 0, z}, voxel::Voxel::Stone3);
        //     }
        // }

        for (i32 x = -256; x < 255; ++x)
        {
            for (i32 z = -256; z < 255; ++z)
            {
                for (i32 y = 0; y < 128; ++y)
                {
                    if (y == 0)
                    {
                        insertVoxelAt({x, y, z}, voxel::Voxel::Stone3);
                    }
                }
            }
        }

        glm::f32vec3 center = {0, 0, 0}; // Center of the structure

        // Trunk of the tree
        for (int y = 0; y < 32; ++y) // Trunk height of 10 voxels
        {
            insertVoxelAt(center + glm::f32vec3(0, y, 0), voxel::Voxel::Stone3);
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
            glm::f32vec3 direction = glm::normalize(
                branch.direction); // Direction vector for the branch

            // Draw the branch (wood voxels)
            for (float i = 0; i < branch.length; ++i)
            {
                glm::f32vec3 pos = start + direction * i;
                insertVoxelAt(pos, voxel::Voxel::Stone0);
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
                        glm::f32vec3 leafPos =
                            leafCenter + glm::f32vec3(x, y, z);
                        if (glm::length(glm::vec3(x, y, z))
                            <= leafRadius) // Check to make it spherical
                        {
                            insertVoxelAt(leafPos, voxel::Voxel::Dirt5);
                        }
                    }
                }
            }
        }

        // Build pillars in a circular formation
        int   numPillars = 12;     // Number of pillars
        float radius     = 192.0f; // Radius of the pillar circle
        for (int i = 0; i < numPillars; ++i)
        {
            float angle = i * (2.0f * glm::pi<float>() / numPillars);

            glm::i32vec3 pillarBase =
                center
                + glm::f32vec3(
                    static_cast<int>(radius * cos(angle)),
                    0,
                    static_cast<int>(radius * sin(angle)));

            for (float theta = 0; theta < 2 * glm::pi<float>(); theta += 0.06)
            {
                for (int r = 0; r < 16; r++)
                {
                    for (int y = 0; y < 64; ++y)
                    {
                        insertVoxelAt(
                            pillarBase
                                + glm::i32vec3(
                                    r * cos(theta), y, r * sin(theta)),
                            voxel::Voxel::Stone1);
                    }
                }
            }
        }

        // Build doughnut ceiling with a hole in the center
        int innerRadius = 128; // Inner radius (hole size)
        int outerRadius = 240; // Outer radius (doughnut size)

        for (int h = 0; h < 24; ++h)
        {
            int currentHeight =
                h + 52; // Start the doughnut at a base height of 20
            for (int x = -outerRadius; x <= outerRadius; ++x)
            {
                for (int z = -outerRadius; z <= outerRadius; ++z)
                {
                    float dist = glm::length(glm::vec2(x, z));

                    // Check if the point is within the doughnut's ring shape
                    // (between inner and outer radius)
                    if (dist >= innerRadius && dist <= outerRadius)
                    {
                        // Add a voxel for this (x, z) position at the current
                        // height layer
                        insertVoxelAt(
                            center + glm::f32vec3(x, currentHeight, z),
                            voxel::Voxel::Stone0);
                    }
                }
            }
        }

        for (int i = 0; i < 1279; ++i)
        {
            this->lights.push_back(this->chunk_manager.createPointLight());
        }

        // std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> ddist {-1.0, 1.0};

        auto genVec3 = [&]() -> glm::vec3
        {
            return glm::vec3 {ddist(gen), ddist(gen), ddist(gen)};
        };

        for (const voxel::PointLight& l : this->lights)
        {
            this->chunk_manager.modifyPointLight(
                l,
                glm::vec4 {
                    genVec3() * glm::vec3 {256.0, 42.0, 256.0}
                        + glm::vec3 {0.0, 41.0, 0.0},
                    0.0},
                glm::vec4 {genVec3() / 2.0f + 0.5f, 64.0},
                glm::vec4 {0.0, 0.7, 1.3, 0.0});
        }

        this->camera.addPosition({-5.0f, 20.0f, -2.5f});
        this->camera.addPitch(0.75);
        this->camera.addYaw(1.87);
    }

    Verdigris::~Verdigris()
    {
        for (voxel::PointLight& l : this->lights)
        {
            this->chunk_manager.destroyPointLight(std::move(l));
        }
    }

    std::pair<game::Camera, std::vector<game::FrameGenerator::RecordObject>>
    Verdigris::onFrame(float deltaTime) const
    {
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

            return glm::i32vec3 {
                static_cast<i32>(x), 64.0, static_cast<i32>(z)};
        };

        const i32 frameNumber =
            static_cast<i32>(this->game->getRenderer()->getFrameNumber());

        auto thisPos = genSpiralPos(frameNumber);
        auto prevPos = genSpiralPos(frameNumber - 1);

        if (thisPos != prevPos)
        {
            for (i32 i = 0; i < 32; ++i)
            {
                this->chunk_manager.writeVoxel(
                    thisPos + glm::i32vec3 {0, i, 0}, voxel::Voxel::Dirt3);
                this->chunk_manager.writeVoxel(
                    prevPos + glm::i32vec3 {0, i, 0},
                    voxel::Voxel::NullAirEmpty);
            }
        }

        // TODO: moving diagonally is faster
        const float moveScale =
            this->game->getRenderer()->getWindow()->isActionActive(
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
                "Frame: {} | {} | {}",
                deltaTime,
                1.0f / deltaTime,
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

        draws.append_range(
            this->chunk_manager.makeRecordObject(this->game, this->camera));

        return {this->camera, std::move(draws)};
    }

} // namespace verdigris
