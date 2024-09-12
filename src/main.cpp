#include "gfx/renderer.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "util/log.hpp"
#include <cstdlib>
#include <exception>
#include <gfx/vulkan/device.hpp>
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
            renderer.recordOnThread([&](std::size_t,
                                        vk::CommandBuffer commandBuffer,
                                        U32,
                                        gfx::vulkan::Swapchain&) {});
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
