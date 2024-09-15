#include "game.hpp"
#include "camera.hpp"
#include "ec/ec_manager.hpp"
#include "frame_generator.hpp"
#include "game/ec/components.hpp"
#include "game/render/render_manager.hpp"
#include "render/triangle_component.hpp"
#include <gfx/renderer.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <gfx/window.hpp>
#include <memory>
#include <shaders/shaders.hpp>
#include <util/log.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game
{
    Game::Game()
        : renderer {std::make_unique<gfx::Renderer>()}
        , frame_generator {std::make_unique<FrameGenerator>(&*this->renderer)}
        , renderable_manager {std::make_unique<render::RenderManager>(&*this)}
        // , ec_manager {std::make_unique<ec::ECManager>()}
        , should_game_keep_ticking {true}
    {
        // const ec::ECManager::Entity entity =
        // this->ec_manager->createEntity(1);

        // this->ec_manager->addComponent(entity, render::TriangleComponent {});
    }

    Game::~Game() noexcept = default;

    void Game::run()
    {
        const Camera workingCamera {glm::vec3 {0.0, 0.0, 0.0}};

        // this->ec_manager->addComponent<ec::FooComponent>({}, {});

        while (!this->renderer->shouldWindowClose())
        {
            // this->ec_manager->flush();

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

    // const ec::ECManager* Game::getECManager() const noexcept
    // {
    //     return this->ec_manager.get();
    // }
} // namespace game
