#include "game.hpp"
#include "camera.hpp"
#include "ec/entity_component_manager.hpp"
#include "frame_generator.hpp"
#include "util/threads.hpp"
#include <gfx/renderer.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <gfx/vulkan/device.hpp>
#include <gfx/window.hpp>
#include <memory>
#include <shaders/shaders.hpp>
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

    Game::Game()
        : renderer {std::make_unique<gfx::Renderer>()}
        , frame_generator {std::make_unique<FrameGenerator>(this)}
        , ec_manager {std::make_unique<ec::EntityComponentManager>()}
        , active_game_state {util::Mutex<std::unique_ptr<GameState>> {nullptr}}
        , should_game_keep_ticking {true}
        , should_game_close {false}
    {
        util::assertFatal(
            globalGame == nullptr, "Don't create multiple Game s");

        globalGame = this;
    }

    Game::~Game() noexcept
    {
        this->renderer->getDevice()->getDevice().waitIdle();
    }

    void Game::run()
    {
        this->renderer->getWindow()->attachCursor();

        while (!this->renderer->shouldWindowClose()
               && !this->should_game_close.load())
        {
            while (!this->renderer->shouldWindowClose()
                   && this->should_game_keep_ticking.load()
                   && !this->should_game_close.load())
            {
                const float deltaTime =
                    this->renderer->getWindow()->getDeltaTimeSeconds();

                this->active_game_state.lock(
                    [&](std::unique_ptr<GameState>& state)
                    {
                        const auto [camera, recordObjects] =
                            state->onFrame(deltaTime);

                        this->frame_generator->generateFrame(
                            camera, recordObjects);
                    });
            }
        }
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
        const vk::Extent2D frameBufferSize =
            this->renderer->getWindow()->getFramebufferSize();

        return static_cast<float>(frameBufferSize.width)
             / static_cast<float>(frameBufferSize.height);
    }

    const gfx::Renderer* Game::getRenderer() const noexcept
    {
        return this->renderer.get();
    }

    const ec::EntityComponentManager*
    Game::getEntityComponentManager() const noexcept
    {
        return this->ec_manager.get();
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
