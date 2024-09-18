#pragma once

#include "util/threads.hpp"
#include <game/camera.hpp>
#include <game/frame_generator.hpp>

namespace gfx
{
    class Renderer;
} // namespace gfx

namespace game::ec
{
    class EntityComponentManager;
} // namespace game::ec

namespace game::render
{
    class Game;

    class RenderManager
    {
    public:
        explicit RenderManager(const game::Game*);
        ~RenderManager() noexcept = default;

        RenderManager(const RenderManager&)             = delete;
        RenderManager(RenderManager&&)                  = delete;
        RenderManager& operator= (const RenderManager&) = delete;
        RenderManager& operator= (RenderManager&&)      = delete;

        void setCamera(Camera) const noexcept;

        std::vector<FrameGenerator::RecordObject> generateFrameObjects();

    private:
        const game::Game*                   game;
        util::Mutex<Camera>                 camera;
        std::shared_ptr<vk::UniquePipeline> triangle_pipeline;
    };
} // namespace game::render
