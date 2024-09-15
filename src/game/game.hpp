#pragma once

#include "game/render/render_manager.hpp"
#include <atomic>
#include <memory>

namespace gfx
{
    class Renderer;
} // namespace gfx

namespace game
{
    namespace ec
    {
        class ECManager;
    } // namespace ec

    namespace render
    {
        class RenderManager;
    } // namespace render

    class FrameGenerator;

    class Game
    {
    public:
        Game();
        ~Game() noexcept;

        Game(const Game&)             = delete;
        Game(Game&&)                  = delete;
        Game& operator= (const Game&) = delete;
        Game& operator= (Game&&)      = delete;

        void                run();
        [[nodiscard]] float getFovXRadians() const noexcept;
        [[nodiscard]] float getFovYRadians() const noexcept;
        [[nodiscard]] float getAspectRatio() const noexcept;

        const gfx::Renderer* getRenderer() const noexcept;
        const ec::ECManager* getECManager() const noexcept;
    private:
        // Rendering abstraction
        std::unique_ptr<gfx::Renderer>         renderer;
        std::unique_ptr<FrameGenerator>        frame_generator;
        std::unique_ptr<render::RenderManager> renderable_manager;

        // std::unique_ptr<ec::ECManager> ec_manager;

        std::atomic<bool> should_game_keep_ticking;
    };
} // namespace game