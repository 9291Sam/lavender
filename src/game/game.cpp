#include "game.hpp"
#include "camera.hpp"
#include "frame_generator.hpp"
#include "util/static_filesystem.hpp"
#include "util/threads.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <gfx/renderer.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <gfx/vulkan/device.hpp>
#include <gfx/window.hpp>
#include <memory>
#include <util/log.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game
{

    namespace
    {
        const Game* globalGame = nullptr; // NOLINT
    } // namespace

    // const Game* getGame()
    // {
    //     return globalGame;
    // }

    Game::GameState::~GameState() = default;

    Game::Game()
        : renderer {std::make_unique<gfx::Renderer>()}
        , frame_generator {std::make_unique<FrameGenerator>(this)}
        , active_game_state {util::Mutex<std::unique_ptr<GameState>> {nullptr}}
        , should_game_keep_ticking {true}
        , should_game_close {false}
    {
        util::assertFatal(globalGame == nullptr, "Don't create multiple Game s");

        globalGame = this;
    }

    Game::~Game() noexcept
    {
        this->renderer->getDevice()->getDevice().waitIdle();
    }

    void Game::run()
    {
        this->renderer->getWindow()->attachCursor();

        std::atomic<bool>                       shouldWorkerStop {false};
        util::Mutex<std::function<void(float)>> tickFunc {};

        std::future<void> tickWorker = std::async(
            [&tickFunc, &shouldWorkerStop]
            {
                float deltaTime = 0.0f;
                auto  start     = std::chrono::high_resolution_clock::now();

                while (!shouldWorkerStop.load())
                {
                    f32TickDeltaTime.store(
                        std::bit_cast<u32>(deltaTime), std::memory_order_relaxed);

                    tickFunc.lock(
                        [&](std::function<void(float)>& func)
                        {
                            if (func != nullptr)
                            {
                                func(deltaTime);
                            }
                        });

                    auto end = std::chrono::high_resolution_clock::now();

                    deltaTime = static_cast<float>((end - start).count()) / 1000000000.0f;
                    start     = end;
                }
            });

        bool isFirstIterationAfterGameStateChange = true;

        while (!this->renderer->shouldWindowClose() && !this->should_game_close.load())
        {
            this->active_game_state.lock(
                [&](std::unique_ptr<GameState>& state)
                {
                    if (!this->should_game_close.load())
                    {
                        if (isFirstIterationAfterGameStateChange)
                        {
                            tickFunc.lock(
                                [&](std::function<void(float)>& f)
                                {
                                    f = [&](float deltaTime)
                                    {
                                        state->onTick(deltaTime);
                                    };
                                });
                        }
                        const float deltaTime = this->renderer->getWindow()->getDeltaTimeSeconds();

                        auto                         start   = std::chrono::steady_clock::now();
                        GameState::OnFrameReturnData package = state->onFrame(deltaTime);
                        auto                         end     = std::chrono::steady_clock::now();

                        std::chrono::duration<float> diff = end - start;

                        this->frame_generator->generateFrame(
                            package.main_scene_camera,
                            std::move(package.record_objects),
                            package.generator.getTasks());

                        isFirstIterationAfterGameStateChange = false;
                    }
                    else
                    {
                        // game state change
                        tickFunc.lock(
                            [&](std::function<void(float)>& f)
                            {
                                f = nullptr;
                            });

                        isFirstIterationAfterGameStateChange = true;
                    }
                });
        }

        shouldWorkerStop = true;

        tickWorker.get();
    }

    float Game::getFovXRadians() const noexcept
    {
        return this->getAspectRatio() * this->getFovYRadians();
    }

    float Game::getFovYRadians() const noexcept // NOLINT: may change
    {
        return glm::radians(70.0f);
    }

    float Game::getAspectRatio() const noexcept
    {
        const vk::Extent2D frameBufferSize = this->renderer->getWindow()->getFramebufferSize();

        return static_cast<float>(frameBufferSize.width)
             / static_cast<float>(frameBufferSize.height);
    }

    const gfx::Renderer* Game::getRenderer() const noexcept
    {
        return this->renderer.get();
    }

    vk::DescriptorSet Game::getGlobalInfoDescriptorSet() const noexcept
    {
        return this->frame_generator->getGlobalInfoDescriptorSet();
    }

    std::shared_ptr<vk::UniqueDescriptorSetLayout>
    Game::getGlobalInfoDescriptorSetLayout() const noexcept
    {
        return this->frame_generator->getGlobalInfoDescriptorSetLayout();
    }

    void Game::terminateGame()
    {
        this->should_game_close = true;
    }
} // namespace game
