#include "renderer.hpp"
#include "gfx/vulkan/frame_manager.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "util/log.hpp"
#include "util/threads.hpp"
#include "vulkan/allocator.hpp"
#include "vulkan/device.hpp"
#include "vulkan/frame_manager.hpp"
#include "vulkan/instance.hpp"
#include "window.hpp"
#include <memory>
#include <vulkan/vulkan_handles.hpp>

namespace gfx
{

    Renderer::Renderer()
    {
        this->window   = std::make_unique<Window>();
        this->instance = std::make_unique<vulkan::Instance>();
        this->surface  = this->window->createSurface(**this->instance);
        this->device =
            std::make_unique<vulkan::Device>(**this->instance, *this->surface);
        this->allocator =
            std::make_unique<vulkan::Allocator>(*this->instance, *this->device);

        this->critical_section = util::Mutex {Renderer::makeCriticalSection(
            *this->device, *this->surface, *this->window)};
    }

    Renderer::~Renderer() noexcept = default;

    bool Renderer::recordOnThread(
        std::function<
            void(std::size_t, vk::CommandBuffer, U32, vulkan::Swapchain&)>
            recordFunc) const
    {
        this->window->beginFrame();

        bool resizeOcurred = false;

        util::logTrace("Frame Begin!");

        this->critical_section.lock(
            [&](std::unique_ptr<RenderingCriticalSection>&
                    lockedCriticalSection)
            {
                const std::expected<void, vulkan::Frame::ResizeNeeded>
                    drawFrameResult =
                        lockedCriticalSection->frame_manager->recordAndDisplay(
                            [&](std::size_t       flyingFrameIdx,
                                vk::CommandBuffer commandBuffer,
                                U32               swapchainImageIdx)
                            {
                                recordFunc(
                                    flyingFrameIdx,
                                    commandBuffer,
                                    swapchainImageIdx,
                                    *lockedCriticalSection->swapchain);
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

        util::logTrace("Frame end!");

        return resizeOcurred;
    }

    bool Renderer::shouldWindowClose() const noexcept
    {
        return this->window->shouldWindowClose();
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
                .swapchain {std::move(swapchain)},
                .frame_manager {std::move(frameManager)}});
    }
} // namespace gfx
