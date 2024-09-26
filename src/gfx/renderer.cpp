#include "renderer.hpp"
#include "gfx/vulkan/frame_manager.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "util/threads.hpp"
#include "vulkan/allocator.hpp"
#include "vulkan/device.hpp"
#include "vulkan/frame_manager.hpp"
#include "vulkan/instance.hpp"
#include "window.hpp"
#include <GLFW/glfw3.h>
#include <memory>
#include <vulkan/vulkan_handles.hpp>

namespace gfx
{

    Renderer::Renderer()
    {
        this->window = std::make_unique<Window>(
            std::map<gfx::Window::Action, gfx::Window::ActionInformation> {
                {Window::Action::PlayerMoveForward,
                 Window::ActionInformation {
                     .key {GLFW_KEY_W},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveBackward,
                 Window::ActionInformation {
                     .key {GLFW_KEY_S},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveLeft,
                 Window::ActionInformation {
                     .key {GLFW_KEY_A},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveRight,
                 Window::ActionInformation {
                     .key {GLFW_KEY_D},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveUp,
                 Window::ActionInformation {
                     .key {GLFW_KEY_SPACE},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveDown,
                 Window::ActionInformation {
                     .key {GLFW_KEY_LEFT_CONTROL},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerSprint,
                 Window::ActionInformation {
                     .key {GLFW_KEY_LEFT_SHIFT},
                     .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::ToggleConsole,
                 Window::ActionInformation {
                     .key {GLFW_KEY_GRAVE_ACCENT},
                     .method {Window::InteractionMethod::SinglePress}}},

                {Window::Action::CloseWindow,
                 Window::ActionInformation {
                     .key {GLFW_KEY_ESCAPE},
                     .method {Window::InteractionMethod::SinglePress}}},

                {Window::Action::ToggleCursorAttachment,
                 Window::ActionInformation {
                     .key {GLFW_KEY_BACKSLASH},
                     .method {Window::InteractionMethod::SinglePress}}},

            },
            vk::Extent2D {1280, 720}, // NOLINT
            "lavender");
        this->instance = std::make_unique<vulkan::Instance>();
        this->surface  = this->window->createSurface(**this->instance);
        this->device =
            std::make_unique<vulkan::Device>(**this->instance, *this->surface);
        this->allocator =
            std::make_unique<vulkan::Allocator>(*this->instance, *this->device);

        this->critical_section = util::Mutex {Renderer::makeCriticalSection(
            *this->device, *this->surface, *this->window)};
    }

    Renderer::~Renderer() noexcept
    {
        this->device->getDevice().waitIdle();
    }

    bool Renderer::recordOnThread(
        std::function<
            void(vk::CommandBuffer, u32, vulkan::Swapchain&, std::size_t)>
            recordFunc) const
    {
        bool resizeOcurred = false;

        this->critical_section.lock(
            [&](std::unique_ptr<RenderingCriticalSection>&
                    lockedCriticalSection)
            {
                const std::expected<void, vulkan::Frame::ResizeNeeded>
                    drawFrameResult =
                        lockedCriticalSection->frame_manager->recordAndDisplay(
                            [&](std::size_t       flyingFrameIdx,
                                vk::CommandBuffer commandBuffer,
                                u32               swapchainImageIdx)
                            {
                                recordFunc(
                                    commandBuffer,
                                    swapchainImageIdx,
                                    *lockedCriticalSection->swapchain,
                                    flyingFrameIdx);
                            });

                if (!drawFrameResult.has_value())
                {
                    this->device->getDevice().waitIdle();

                    lockedCriticalSection.reset();

                    lockedCriticalSection = Renderer::makeCriticalSection(
                        *this->device, *this->surface, *this->window);

                    resizeOcurred = true;
                }
            });

        this->allocator->trimCaches();

        this->window->endFrame();

        return resizeOcurred;
    }

    bool Renderer::shouldWindowClose() const noexcept
    {
        return this->window->shouldClose();
    }

    std::unique_ptr<Renderer::RenderingCriticalSection>
    Renderer::makeCriticalSection(
        const vulkan::Device& device,
        vk::SurfaceKHR        surface,
        const Window&         window)
    {
        std::unique_ptr<vulkan::Swapchain> swapchain =
            std::make_unique<vulkan::Swapchain>(
                device, surface, window.getFramebufferSize());

        std::unique_ptr<vulkan::FrameManager> frameManager =
            std::make_unique<vulkan::FrameManager>(device, **swapchain);

        return std::make_unique<RenderingCriticalSection>(
            RenderingCriticalSection {
                .frame_manager {std::move(frameManager)},
                .swapchain {std::move(swapchain)},
            });
    }

    const vulkan::Device* Renderer::getDevice() const noexcept
    {
        return &*this->device;
    }

    const vulkan::Allocator* Renderer::getAllocator() const noexcept
    {
        return &*this->allocator;
    }

    const Window* Renderer::getWindow() const noexcept
    {
        return &*this->window;
    }
} // namespace gfx
