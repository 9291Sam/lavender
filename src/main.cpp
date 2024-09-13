#include "gfx/renderer.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "util/log.hpp"
#include <cstdlib>
#include <exception>
#include <gfx/vulkan/device.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        gfx::Renderer renderer {};

        while (!renderer.shouldWindowClose())
        {
            renderer.recordOnThread(
                [&](std::size_t,
                    vk::CommandBuffer       commandBuffer,
                    U32                     swapchainImageIdx,
                    gfx::vulkan::Swapchain& swapchain)
                {
                    ;

                    commandBuffer.clearColorImage(
                        swapchain.getImages()[swapchainImageIdx],
                        vk::ImageLayout::eUndefined,
                        vk::ClearColorValue {.float32 {{0.0, 1.0, 0.0, 1.0}}},
                        vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}});
                });
        }
    }
    catch (const std::exception& e)
    {
        util::logFatal("Lavender has crashed! | {}", e.what());
    }
    catch (...)
    {
        util::logFatal("Lavender has crashed! | Non Exception Thrown!");
    }

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
