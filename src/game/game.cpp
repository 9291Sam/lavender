#include "game.hpp"
#include "camera.hpp"
#include "ec/entity_component_manager.hpp"
#include "frame_generator.hpp"
#include "game/ec/components.hpp"
#include "game/render/render_manager.hpp"
#include "game/transform.hpp"
#include "render/triangle_component.hpp"
#include <gfx/renderer.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <gfx/vulkan/device.hpp>
#include <gfx/window.hpp>
#include <memory>
#include <random>
#include <shaders/shaders.hpp>
#include <util/log.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game
{
    Game::Game()
        : renderer {std::make_unique<gfx::Renderer>()}
        , frame_generator {std::make_unique<FrameGenerator>(&*this->renderer)}
        , renderable_manager {std::make_unique<render::RenderManager>(&*this)}
        , ec_manager {std::make_unique<ec::EntityComponentManager>()}
        , should_game_keep_ticking {true}
    {
        const ec::Entity entity = this->ec_manager->createEntity();

        this->ec_manager->addComponent(entity, render::TriangleComponent {});
        this->ec_manager->tryAddComponent(entity, render::TriangleComponent {});

        this->ec_manager->destroyEntity(entity);
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
    }

    Game::~Game() noexcept
    {
        this->renderer->getDevice()->getDevice().waitIdle();
    }

    void Game::run()
    {
        this->renderer->getWindow()->attachCursor();

        Camera workingCamera {glm::vec3 {0.0, 0.0, 0.0}};
        // this->game.renderer.getMenu().setPlayerPosition(
        //     workingCamera.getPosition());

        // this->ec_manager->addComponent<ec::FooComponent>({}, {});

        std::mt19937_64                 gen {std::random_device {}()};
        std::normal_distribution<float> dist {64, 3};

        while (!this->renderer->shouldWindowClose()
               && this->should_game_keep_ticking.load())
        {
            const float deltaTime =
                this->renderer->getWindow()->getDeltaTimeSeconds();

            // TODO: moving diaginally is faster
            const float moveScale = this->renderer->getWindow()->isActionActive(
                                        gfx::Window::Action::PlayerSprint)
                                      ? 50.0f
                                      : 10.0f;
            const float rotateSpeedScale = 6.0f;

            if (this->renderer->getWindow()->isActionActive(
                    gfx::Window::Action::CloseWindow))
            {
                this->should_game_keep_ticking = false;
            }

            workingCamera.addPosition(
                workingCamera.getForwardVector() * deltaTime * moveScale
                * (this->renderer->getWindow()->isActionActive(
                       gfx::Window::Action::PlayerMoveForward)
                       ? 1.0f
                       : 0.0f));

            workingCamera.addPosition(
                -workingCamera.getForwardVector() * deltaTime * moveScale
                * (this->renderer->getWindow()->isActionActive(
                       gfx::Window::Action::PlayerMoveBackward)
                       ? 1.0f
                       : 0.0f));

            workingCamera.addPosition(
                -workingCamera.getRightVector() * deltaTime * moveScale
                * (this->renderer->getWindow()->isActionActive(
                       gfx::Window::Action::PlayerMoveLeft)
                       ? 1.0f
                       : 0.0f));

            workingCamera.addPosition(
                workingCamera.getRightVector() * deltaTime * moveScale
                * (this->renderer->getWindow()->isActionActive(
                       gfx::Window::Action::PlayerMoveRight)
                       ? 1.0f
                       : 0.0f));

            workingCamera.addPosition(
                Transform::UpVector * deltaTime * moveScale
                * (this->renderer->getWindow()->isActionActive(
                       gfx::Window::Action::PlayerMoveUp)
                       ? 1.0f
                       : 0.0f));

            workingCamera.addPosition(
                -Transform::UpVector * deltaTime * moveScale
                * (this->renderer->getWindow()->isActionActive(
                       gfx::Window::Action::PlayerMoveDown)
                       ? 1.0f
                       : 0.0f));

            auto getMouseDeltaRadians = [&]
            {
                // each value from -1.0 -> 1.0 representing how much it moved on
                // the screen
                const auto [nDeltaX, nDeltaY] =
                    this->renderer->getWindow()->getScreenSpaceMouseDelta();

                const auto deltaRadiansX =
                    (nDeltaX / 2) * this->getFovXRadians();
                const auto deltaRadiansY =
                    (nDeltaY / 2) * this->getFovYRadians();

                return gfx::Window::Delta {
                    .x {deltaRadiansX}, .y {deltaRadiansY}};
            };

            auto [xDelta, yDelta] = getMouseDeltaRadians();

            workingCamera.addYaw(xDelta * rotateSpeedScale);
            workingCamera.addPitch(yDelta * rotateSpeedScale);

            auto get = [&]
            {
                return dist(gen);
            };

            this->ec_manager->addComponent(
                this->ec_manager->createEntity(),
                render::TriangleComponent {.transform {Transform {
                    .rotation {glm::quat {glm::vec3 {get(), get(), get()}}},
                    .translation {glm::vec3 {get(), get(), get()}},
                    .scale {get() * 3, get() * -3, get() * 8},
                }}});

            this->renderable_manager->setCamera(workingCamera);

            std::vector<FrameGenerator::RecordObject> recordObjects =
                this->renderable_manager->generateFrameObjects();

            this->frame_generator->generateFrame(recordObjects);
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
} // namespace game
