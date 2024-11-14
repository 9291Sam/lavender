#pragma once

#include "frame_generator.hpp"
#include "game/camera.hpp"
#include <atomic>
#include <concepts>
#include <memory>
#include <util/threads.hpp>

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

    // static const Game* getGame();

    class Game
    {
    public:
        struct GameState
        {
            GameState() = default;
            virtual ~GameState();

            GameState(const GameState&)             = delete;
            GameState(GameState&&)                  = delete;
            GameState& operator= (const GameState&) = delete;
            GameState& operator= (GameState&&)      = delete;

            [[nodiscard]] virtual std::pair<Camera, std::vector<FrameGenerator::RecordObject>>
                         onFrame(float) const = 0;
            virtual void onTick() const       = 0;
        };

    public:
        Game();
        ~Game() noexcept;

        Game(const Game&)             = delete;
        Game(Game&&)                  = delete;
        Game& operator= (const Game&) = delete;
        Game& operator= (Game&&)      = delete;

        [[nodiscard]] float getFovXRadians() const noexcept;
        [[nodiscard]] float getFovYRadians() const noexcept;
        [[nodiscard]] float getAspectRatio() const noexcept;

        const gfx::Renderer*              getRenderer() const noexcept;
        const ec::EntityComponentManager* getEntityComponentManager() const noexcept;
        std::shared_ptr<vk::UniqueDescriptorSetLayout>
                          getGlobalInfoDescriptorSetLayout() const noexcept;
        vk::DescriptorSet getGlobalInfoDescriptorSet() const noexcept;

        template<std::derived_from<GameState> T>
        void loadGameState()
        {
            this->should_game_keep_ticking.store(false);

            this->active_game_state.lock(
                [&](std::unique_ptr<GameState>& gameState)
                {
                    gameState.reset();

                    gameState = std::make_unique<T>(this);
                });

            this->should_game_keep_ticking.store(true);
        }
        void terminateGame();
        void run();

    private:
        // Rendering abstraction
        std::unique_ptr<gfx::Renderer>  renderer;
        std::unique_ptr<FrameGenerator> frame_generator;

        std::unique_ptr<ec::EntityComponentManager> ec_manager;

        util::Mutex<std::unique_ptr<GameState>> active_game_state;

        std::atomic<bool> should_game_keep_ticking;
        std::atomic<bool> should_game_close;
    };
} // namespace game
