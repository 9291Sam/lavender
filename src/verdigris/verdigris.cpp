
#include "verdigris.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "gfx/profiler/profiler.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/window.hpp"
#include "skybox_manager.hpp"
#include "triangle_component.hpp"
#include "util/atomic.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/static_filesystem.hpp"
#include "verdigris/flyer.hpp"
#include "voxel/constants.hpp"
#include "voxel/point_light.hpp"
#include "voxel/voxel.hpp"
#include "voxel/world.hpp"
#include "world/generator.hpp"
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
        , voxel_world {util::Mutex<voxel::World> {
              voxel::World {std::make_unique<world::WorldChunkGenerator>(38484334), this->game}}}
        , triangle {ecs::createEntity()}

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

        // auto genFunc = [](i32 x, i32 z) -> i32
        // {
        //     return static_cast<i32>(8 * std::sin(x / 24.0) + 8 * std::cos(z / 24.0) + 32.0) -
        //     128;
        // };
        // std::mt19937_64                       gen {std::random_device {}()};
        // std::uniform_real_distribution<float> dist {-1.0, 1.0};

        // auto genVec3 = [&]() -> glm::vec3
        // {
        //     return glm::vec3 {
        //         32.0f * std ::sin(dist(gen)) * dist(gen),
        //         (32.0f * std ::sin(dist(gen)) * dist(gen)) + 80.0f,
        //         32.0f * std ::sin(dist(gen)) * dist(gen)};
        // };

        voxel::PointLight eLight {};

        this->voxel_world.lock(
            [&](voxel::World& w)
            {
                this->lights.push_back(w.createPointLight(
                    {66, 166, -33}, {1.0, 1.0, 1.0, 384.0}, {0.0, 0.0, 0.025, 0.0}));

                this->lights.push_back(w.createPointLight(
                    {105, 37, 104}, {1.0, 1.0, 1.0, 384}, {0.0, 0.0, 0.025f, 0.0}));

                eLight = w.createPointLight(
                    {105, 37, 104}, {1.0, 1.0, 1.0, 84}, {0.0, 0.0, 0.025f, 0.0});
            });

        this->triangle.addComponent(TriangleComponent {});
        this->triangle.addComponent<voxel::PointLight>(std::move(eLight));

        for (int j = 0; j < 4080; ++j)
        {
            ecs::UniqueEntity e = ecs::createEntity();

            const i32 x = j % 64;
            const i32 y = j / 64;

            e.addComponent(VoxelComponent {
                .voxel {voxel::Voxel::Emerald}, .position {voxel::WorldPosition {{x, y + 64, 0}}}});

            this->voxels.push_back(std::move(e));
        }

        // CpuMasterBuffer
        // meshoperation (copy of everything)
        // once its done upload it to the gpu

        this->camera.addPosition({79.606, 375.586, 16.78});
        this->camera.addPitch(-0.12f);
        this->camera.addYaw(4.87f);
    }

    Verdigris::~Verdigris()
    {
        this->voxel_world.lock(
            [&](voxel::World& w)
            {
                this->triangle.mutateComponent<voxel::PointLight>(
                    [&](voxel::PointLight& l)
                    {
                        w.destroyPointLight(std::move(l));
                    });

                for (voxel::PointLight& l : this->lights)
                {
                    w.destroyPointLight(std::move(l));
                }
            });
    }

    game::Game::GameState::OnFrameReturnData Verdigris::onFrame(float deltaTime) const
    {
        gfx::profiler::TaskGenerator profilerTaskGenerator {};

        this->frame_times.push_back(deltaTime);

        if (this->frame_times.size() > 128)
        {
            this->frame_times.pop_front();
        }

        const float averageFrameTime = boost::accumulate(this->frame_times, 0.0f)
                                     / static_cast<float>(this->frame_times.size());

        util::atomicAbaAdd(this->time_alive, deltaTime);

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

        if (this->game->getRenderer()->getFrameNumber() == 4
            || this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ResetPlayPosition))
        {
            this->camera.setPosition({79.606, 75.586, 16.78});
        }

        static float verticalVelocity = 0.0f;    // Initial velocity for gravity
        const float  gravity          = -512.0f; // Gravitational acceleration (m/s²)
        const float  maxFallSpeed     = -512.0f; // Terminal velocity

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

        if (this->game->getRenderer()->getWindow()->isActionActive(gfx::Window::Action::SpawnFlyer))
        {
            this->flyers.push_back(Flyer {
                this->camera.getPosition() + glm::vec3 {0.0f, 32.0f, 0.0f},
                this->camera.getForwardVector(),
                pDist(gen),
                static_cast<voxel::Voxel>(rand() % 12 + 1)});
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveUp))
        {
            if (verticalVelocity == 0.0f)
            {
                verticalVelocity += 128.0f;
            }
        }

        newPosition.y = previousPosition.y;

        profilerTaskGenerator.stamp("Player Movement", gfx::profiler::Carrot);

        this->voxel_world.lock(
            [&](voxel::World& w)
            {
                for (Flyer& f : this->flyers)
                {
                    f.update(deltaTime);
                    f.display(w);
                }

                glm::vec3 currentPosition = this->camera.getPosition();
                glm::vec3 displacement    = newPosition - currentPosition;

                verticalVelocity += gravity * deltaTime;
                verticalVelocity = std::max(verticalVelocity, maxFallSpeed);

                displacement.y += verticalVelocity * deltaTime;

                // HACK: just prevent it from mattering lol
                if (glm::length(displacement) > 1.0f)
                {
                    displacement = glm::normalize(displacement) * 0.999999f;
                }

                glm::vec3 resolvedPosition = currentPosition;

                glm::vec3 testPositionX = resolvedPosition + glm::vec3(displacement.x, 0.0f, 0.0f);
                if (!w.readVoxelOpacity(voxel::WorldPosition {glm::floor(testPositionX)}))
                {
                    resolvedPosition.x += displacement.x;
                }

                glm::vec3 testPositionY = resolvedPosition + glm::vec3(0.0f, displacement.y, 0.0f);
                if (!w.readVoxelOpacity(voxel::WorldPosition {glm::floor(testPositionY)}))
                {
                    resolvedPosition.y += displacement.y;
                }
                else
                {
                    verticalVelocity = 0.0f;

                    glm::vec3 upwardPosition = glm::floor(newPosition);
                    bool      tooFar         = false;

                    int steps = 0;
                    while (!tooFar
                           && w.readVoxelOpacity(voxel::WorldPosition {glm::floor(upwardPosition)}))
                    {
                        upwardPosition.y += 1.0f;

                        if (steps++ > 3)
                        {
                            tooFar = true;
                        }
                    }

                    if (!tooFar)
                    {
                        resolvedPosition.y = upwardPosition.y;
                    }
                }

                glm::vec3 testPositionZ = resolvedPosition + glm::vec3(0.0f, 0.0f, displacement.z);
                if (!w.readVoxelOpacity(voxel::WorldPosition {glm::floor(testPositionZ)}))
                {
                    resolvedPosition.z += displacement.z;
                }

                this->camera.setPosition(resolvedPosition);
            });

        profilerTaskGenerator.stamp("Flyer Update & Apply Movement", gfx::profiler::BelizeHole);

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

        // std::normal_distribution<float> ddist {0.0, 1.14f};
        // std::uniform_real_distribution<float> ddist {-1.0, 1.0};

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

        profilerTaskGenerator.stamp("TriangleUpdate and enqueue", gfx::profiler::Wisteria);

        this->voxel_world.lock(
            [&](voxel::World& w)
            {
                if (this->game->getRenderer()->getFrameNumber() == 0)
                {
                    for (int i = 0; i < 2048; ++i)
                    {
                        const glm::vec3 dir = glm::normalize(glm::ballRand(1.0f));

                        this->flyers.push_back(Flyer {glm::vec3 {0.0, 32.0, 0.0}, dir, 10.0f});
                    }
                }

                const glm::vec3 pos = glm::vec3 {
                    78.0f * std::cos(this->time_alive),
                    (4.0f * std::sin(this->time_alive)) + 122.0f,
                    78.0f * std::sin(this->time_alive)};

                this->triangle.mutateComponent<TriangleComponent>(
                    [&](TriangleComponent& t)
                    {
                        t.transform.translation = pos;
                        t.transform.scale       = glm::vec3 {4.0f};
                    });

                this->triangle.mutateComponent<voxel::PointLight>(
                    [&](voxel::PointLight& l)
                    {
                        w.modifyPointLight(l, pos, {1.0, 1.0, 1.0, 1312.0}, {0.0, 0.0, 0.025, 0.0});
                    });

                // auto thisPos = genSpiralPos(this->game->getRenderer()->getFrameNumber());
                // auto prevPos = genSpiralPos(this->game->getRenderer()->getFrameNumber() -
                // 1);

                // if (thisPos != prevPos)
                // {
                //     for (i32 j = 0; j < 32; ++j)
                //     {
                //         w.writeVoxel(
                //             voxel::WorldPosition {thisPos + glm::i32vec3 {0, j, 0}},
                //             voxel::Voxel::Pearl);
                //         w.writeVoxel(
                //             voxel::WorldPosition {prevPos + glm::i32vec3 {0, j, 0}},
                //             voxel::Voxel::NullAirEmpty);
                //     }
                // }

                w.setCamera(this->camera);

                profilerTaskGenerator.stamp("camera update", gfx::profiler::Nephritis);

                std::vector<game::FrameGenerator::RecordObject> worldRecordObjects =
                    w.getRecordObjects(
                        this->game, this->game->getRenderer()->getStager(), profilerTaskGenerator);

                draws.insert(
                    draws.begin(),
                    std::make_move_iterator(worldRecordObjects.begin()),
                    std::make_move_iterator(worldRecordObjects.end()));

                profilerTaskGenerator.stamp("get Record Objects", gfx::profiler::Silver);
            });

        auto realCamera = this->camera;
        realCamera.addPosition({0.0, 32.0f, 0.0});

        return OnFrameReturnData {
            .main_scene_camera {
                realCamera,
            },
            .record_objects {std::move(draws)},
            .profiler_timestamps {profilerTaskGenerator.getTasks()}};
    }

    void Verdigris::onTick(float) const
    {
        i32 i = 0;

        std::vector<voxel::World::VoxelWrite> writes {};

        ecs::getGlobalECSManager()->iterateComponents<VoxelComponent>(
            [&](ecs::RawEntity, VoxelComponent& c)
            {
                const i32 x = i % 80;
                const i32 y = i / 80;

                const voxel::Voxel newMaterial = [&]
                {
                    // switch (y / (50 / 6))
                    // {
                    // case 0:
                    //     return voxel::Voxel::Amethyst;
                    // case 1:
                    //     return voxel::Voxel::Sapphire;
                    // case 2:
                    //     return voxel::Voxel::Emerald;
                    // case 3:
                    //     return voxel::Voxel::Gold;
                    // case 4:
                    //     return voxel::Voxel::Topaz;
                    // case 5:
                    //     return voxel::Voxel::Ruby;
                    // default:
                    //     return voxel::Voxel::NullAirEmpty;
                    // }
                    return static_cast<voxel::Voxel>((y % 12) + 1);
                }();

                const voxel::WorldPosition newPosition {
                    {x,
                     y + 64,
                     static_cast<i32>(
                         -48.0f
                         + (4.0f
                            * std::sin(
                                (static_cast<float>(x) / 4.0f) + (this->time_alive * 4.0f))))}};

                writes.push_back({c.position, voxel::Voxel::NullAirEmpty});

                c.position = newPosition;
                c.voxel    = newMaterial;

                writes.push_back({c.position, c.voxel});

                i += 1;
            });

        this->voxel_world.lock(
            [&](voxel::World& w)
            {
                w.writeVoxel(writes);
            });
    }

} // namespace verdigris
