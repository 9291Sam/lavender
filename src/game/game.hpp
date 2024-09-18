#pragma once

#include "game/ec/components.hpp"
#include "game/ec/entity.hpp"
#include "game/render/render_manager.hpp"
#include "util/log.hpp"
#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>

namespace gfx
{
    class Renderer;
} // namespace gfx

namespace game
{
    namespace ec
    {
        class EntityComponentManager;
    } // namespace ec

    namespace render
    {
        class RenderManager;
    } // namespace render

    class FrameGenerator;

    class Game
    {
    public:
        struct GameState
        {
            virtual ~GameState() {};

            [[nodiscard]] virtual Camera onFrame(float) const = 0;
            virtual void                 onTick() const       = 0;
        };
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
        const ec::EntityComponentManager*
        getEntityComponentManager() const noexcept;

        template<std::derived_from<GameState> T>
        void loadGameState()
        {
            this->should_game_keep_ticking.store(false);

            util::logTrace("before loadGameState lock");
            this->active_game_state.lock(
                [&](std::unique_ptr<GameState>& gameState)
                {
                    gameState.reset();

                    gameState = std::make_unique<T>(this);
                });

            this->should_game_keep_ticking.store(true);
        }

        void terminateGame()
        {
            this->should_game_close = true;
        }

    private:
        // Rendering abstraction
        // lowest level - gfx::Renderer constructs the command buffer
        // middle level - FrameGenerator records the command buffer
        // highest level - render::RenderManager grabs all things that need
        // to be drawn before forwarding them to FrameGenerator
        std::unique_ptr<gfx::Renderer>         renderer;
        std::unique_ptr<FrameGenerator>        frame_generator;
        std::unique_ptr<render::RenderManager> renderable_manager;

        std::unique_ptr<ec::EntityComponentManager> ec_manager;

        util::Mutex<std::unique_ptr<GameState>> active_game_state;

        std::atomic<bool> should_game_keep_ticking;
        std::atomic<bool> should_game_close;
    };
} // namespace game
