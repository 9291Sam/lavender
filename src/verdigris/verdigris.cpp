
#include "verdigris.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/game.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/window.hpp"
#include "triangle_component.hpp"
#include "util/misc.hpp"
#include "util/static_filesystem.hpp"
#include "voxel/constants.hpp"
#include "voxel/point_light.hpp"
#include "voxel/voxel.hpp"
#include "voxel/world.hpp"
#include "world/generator.hpp"
#include <FastNoise/FastNoise.h>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <glm/fwd.hpp>
#include <numbers>
#include <numeric>
#include <random>
#include <utility>

namespace verdigris
{
    Verdigris::Verdigris(game::Game* game_) // NOLINT
        : game {game_}
        , voxel_world(std::make_unique<world::WorldChunkGenerator>(38484334), this->game)
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

        this->triangle.addComponent(TriangleComponent {});

        // auto genFunc = [](i32 x, i32 z) -> i32
        // {
        //     return static_cast<i32>(8 * std::sin(x / 24.0) + 8 * std::cos(z / 24.0) + 32.0) -
        //     128;
        // };
        std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> ddist {-1.0, 1.0};

        auto genVec3 = [&]() -> glm::vec3
        {
            return glm::vec3 {
                32.0f * std ::sin(ddist(gen)) * ddist(gen),
                32.0f * std ::sin(ddist(gen)) * ddist(gen) + 80.0f,
                32.0f * std ::sin(ddist(gen)) * ddist(gen)};
        };

        for (int j = 0; j < 867; ++j)
        {
            ecs::UniqueEntity e = ecs::getGlobalECSManager()->createEntity();

            TriangleComponent t {};
            t.transform.translation = genVec3();
            t.transform.scale       = glm::vec3 {8.0f};

            e.addComponent(t);

            this->triangles.push_back(std::move(e));
        }

        for (int j = 0; j < 4096; ++j)
        {
            ecs::UniqueEntity e = ecs::createEntity();

            e.addComponent(VoxelComponent {
                .voxel {voxel::Voxel::Emerald},
                .position {voxel::WorldPosition {
                    static_cast<glm::ivec3>(genVec3()) + glm::ivec3 {0, 96, 0}}}});

            util::logTrace("{}", e.getComponent<VoxelComponent>().position.x);

            this->voxels.push_back(std::move(e));
        }

        // CpuMasterBuffer
        // meshoperation (copy of everything)
        // once its done upload it to the gpu

        this->camera.addPosition({79.606, 75.586, 16.78});
        this->camera.addPitch(-0.12f);
        this->camera.addYaw(4.87f);

        for (int i = 0; i < 3; ++i)
        {
            this->lights.push_back(this->voxel_world.createPointLight({}, {}, {}));
        }

        this->lights.push_back(this->voxel_world.createPointLight(
            {105, 315, 104}, {1.0, 1.0, 1.0, 384}, {0.0, 0.0, 0.025f, 0.0}));
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
            std::accumulate(this->frame_times.cbegin(), this->frame_times.cend(), 0.0f)
            / static_cast<float>(this->frame_times.size());

        this->time_alive += deltaTime;
        std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> pDist {-1.0, 1.0};

        // auto genVec3 = [&]() -> glm::vec3
        // {
        //     return glm::vec3 {pDist(gen), pDist(gen), pDist(gen)};
        // };

        auto genSpiralPos = [](u32 f)
        {
            const float t = static_cast<float>(f) / 256.0f;

            const float x = 32.0f * std::sin(t);
            const float z = 32.0f * std::cos(t);

            return glm::i32vec3 {static_cast<i32>(x), 66.0, static_cast<i32>(z)};
        };

        // const i32 frameNumber = static_cast<i32>(this->game->getRenderer()->getFrameNumber());

        // util::logTrace("modify light");
        int i = 0;
        for (const voxel::PointLight& l : this->lights)
        {
            const float offset =
                this->time_alive * 2 + static_cast<float>(i) * std::numbers::pi_v<float> / 2.0f;

            if (i == 0)
            {
                const glm::vec3 pos = glm::vec3 {
                    78.0f * std::cos(offset),
                    4.0f * std::sin(offset) + 122.0f,
                    78.0f * std::sin(offset)};

                // util::logTrace("modify light2");

                this->voxel_world.modifyPointLight(
                    l, pos, {1.0, 1.0, 1.0, 512.0}, {0.0, 0.0, 0.025, 0.0});

                this->triangle.mutateComponent<TriangleComponent>(
                    [&](TriangleComponent& t)
                    {
                        t.transform.translation = pos;
                        t.transform.scale       = glm::vec3 {4.0f};
                    });
            }
            else
            {
                const glm::vec3 pos = glm::vec3 {
                    256.0f * std::cos(38.4f * 1.384f + 93.4f),
                    4.0f * std::sin(38.4f * 1.384f + 93.4f) + 147.0f,
                    256.0f * std::sin(38.4f * 1.384f + 93.4f)};

                this->voxel_world.modifyPointLight(
                    l, pos, {1.0, 1.0, 1.0, 2048.0}, {0.0, 0.0, 0.025, 0.0});
            }

            i += 1;
        }

        auto thisPos = genSpiralPos(this->game->getRenderer()->getFrameNumber());
        auto prevPos = genSpiralPos(this->game->getRenderer()->getFrameNumber() - 1);

        if (thisPos != prevPos)
        {
            for (i32 j = 0; j < 32; ++j)
            {
                this->voxel_world.writeVoxel(
                    voxel::WorldPosition {thisPos + glm::i32vec3 {0, j, 0}}, voxel::Voxel::Pearl);
                this->voxel_world.writeVoxel(
                    voxel::WorldPosition {prevPos + glm::i32vec3 {0, j, 0}},
                    voxel::Voxel::NullAirEmpty);
            }
        }

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

        std::uniform_real_distribution<float> ddist {-1.0, 1.0};

        auto genVec3 = [&]() -> glm::vec3
        {
            return glm::vec3 {
                48.0f * std ::sin(ddist(gen)) * ddist(gen),
                48.0f * std ::sin(ddist(gen)) * ddist(gen) + 80.0f,
                48.0f * std ::sin(ddist(gen)) * ddist(gen)};
        };

        ecs::getGlobalECSManager()->iterateComponents<VoxelComponent>(
            [&](ecs::RawEntity, const VoxelComponent& c)
            {
                this->voxel_world.writeVoxel(c.position, voxel::Voxel::NullAirEmpty);

                const_cast<VoxelComponent&>(c).position = voxel::WorldPosition {
                    static_cast<glm::ivec3>(genVec3())
                    + 5 * genSpiralPos(this->game->getRenderer()->getFrameNumber() * 4)
                    + glm::ivec3 {0, -296, 0}};

                this->voxel_world.writeVoxel(c.position, c.voxel);
            });

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

        this->voxel_world.setCamera(this->camera);

        draws.append_range(
            this->voxel_world.getRecordObjects(this->game, this->game->getRenderer()->getStager()));

        return {this->camera, std::move(draws)};
    }

} // namespace verdigris
