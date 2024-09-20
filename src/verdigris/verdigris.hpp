#pragma once

#include "game/camera.hpp"
#include "game/ec/entity_component_manager.hpp"
#include "game/game.hpp"
#include "game/render/triangle_component.hpp"
#include "gfx/renderer.hpp"
#include "gfx/window.hpp"
#include <random>

namespace verdigris
{
    class Verdigris : public game::Game::GameState
    {
    public:
        mutable game::Camera                    camera;
        game::Game*                             game;
        const game::ec::EntityComponentManager* ec_manager;

        explicit Verdigris(game::Game* game_)
            : game {game_}
            , ec_manager {game_->getEntityComponentManager()}
        {
            const game::ec::Entity entity =
                this->game->getEntityComponentManager()->createEntity();

            this->ec_manager->addComponent(
                entity, game::render::TriangleComponent {});
            // this->ec_manager->addComponent(entity, render::TriangleComponent
            // {});

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

            std::mt19937_64                 gen {std::random_device {}()};
            std::normal_distribution<float> dist {64, 3};

            auto get = [&]
            {
                return dist(gen);
            };

            for (int i = 0; i < 1024; ++i)
            {
                this->ec_manager->addComponent(
                    this->ec_manager->createEntity(),
                    game::render::TriangleComponent {
                        .transform {game::Transform {
                            .rotation {
                                glm::quat {glm::vec3 {get(), get(), get()}}},
                            .translation {glm::vec3 {get(), get(), get()}},
                            .scale {get() * 3, get() * -3, get() * 8},
                        }}});
            }
        }

        ~Verdigris() override = default;

        game::Camera onFrame(float deltaTime) const override
        {
            // TODO: moving diagonally is faster
            const float moveScale =
                this->game->getRenderer()->getWindow()->isActionActive(
                    gfx::Window::Action::PlayerSprint)
                    ? 50.0f
                    : 10.0f;
            const float rotateSpeedScale = 6.0f;

            if (this->game->getRenderer()->getWindow()->isActionActive(
                    gfx::Window::Action::CloseWindow))
            {
                this->game->terminateGame();
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
                    this->game->getRenderer()
                        ->getWindow()
                        ->getScreenSpaceMouseDelta();

                const auto deltaRadiansX =
                    (nDeltaX / 2) * this->game->getFovXRadians();
                const auto deltaRadiansY =
                    (nDeltaY / 2) * this->game->getFovYRadians();

                return gfx::Window::Delta {
                    .x {deltaRadiansX}, .y {deltaRadiansY}};
            };

            auto [xDelta, yDelta] = getMouseDeltaRadians();

            this->camera.addYaw(xDelta * rotateSpeedScale);
            this->camera.addPitch(yDelta * rotateSpeedScale);

            return this->camera;
        }

        void onTick() const override {}
    };
} // namespace verdigris