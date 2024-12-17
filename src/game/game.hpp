#pragma once

#include "frame_generator.hpp"
#include "game/camera.hpp"
#include "gfx/profiler/profiler.hpp"
#include "gfx/profiler/task_generator.hpp"
#include <atomic>
#include <concepts>
#include <ctti/nameof.hpp>
#include <memory>
#include <type_traits>
#include <util/threads.hpp>

static inline gfx::profiler::TaskGenerator* tg = nullptr;

namespace gfx
{
    class Renderer;
} // namespace gfx

namespace game
{

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
        public:
            struct OnFrameReturnData
            {
                Camera                                    main_scene_camera;
                std::vector<FrameGenerator::RecordObject> record_objects;
                gfx::profiler::TaskGenerator              generator;
            };
        public:
            GameState() = default;
            virtual ~GameState();

            GameState(const GameState&)             = delete;
            GameState(GameState&&)                  = delete;
            GameState& operator= (const GameState&) = delete;
            GameState& operator= (GameState&&)      = delete;

            [[nodiscard]] virtual OnFrameReturnData onFrame(float) const = 0;
            virtual void                            onTick(float) const  = 0;
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

        const gfx::Renderer* getRenderer() const noexcept;
        std::shared_ptr<vk::UniqueDescriptorSetLayout>
                          getGlobalInfoDescriptorSetLayout() const noexcept;
        vk::DescriptorSet getGlobalInfoDescriptorSet() const noexcept;

        template<std::derived_from<GameState> T>
            requires std::is_constructible_v<T, Game*>
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
            util::logLog("Loading new GameState: {}", ctti::nameof<T>().cppstring());
        }
        void terminateGame();
        void run();

    private:
        // Rendering abstraction
        std::unique_ptr<gfx::Renderer>  renderer;
        std::unique_ptr<FrameGenerator> frame_generator;

        util::Mutex<std::unique_ptr<GameState>> active_game_state;

        std::atomic<bool> should_game_keep_ticking;
        std::atomic<bool> should_game_close;
    };
} // namespace game
