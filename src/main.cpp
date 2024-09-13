#include "gfx/renderer.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "util/log.hpp"
#include <cstdlib>
#include <exception>
#include <game/frame/frame_generator.hpp>
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
                [&](vk::CommandBuffer       commandBuffer,
                    U32                     swapchainImageIdx,
                    gfx::vulkan::Swapchain& swapchain)
                {
                    game::frame::generateFrame(
                        commandBuffer,
                        swapchainImageIdx,
                        swapchain,
                        renderer.getDevice(),
                        {});
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

    util::logLog("lavender exited successfully!");

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
