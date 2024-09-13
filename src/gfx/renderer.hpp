#pragma once

#include "util/misc.hpp"
#include <functional>
#include <memory>
#include <util/threads.hpp>
#include <vulkan/vulkan.hpp>

namespace gfx
{
    class Window;

    namespace vulkan
    {
        class Instance;
        class Device;
        class Allocator;
        class FrameManager;
        class Swapchain;
    } // namespace vulkan

    class Renderer
    {
    public:
        static constexpr vk::SurfaceFormatKHR ColorFormat =
            vk::SurfaceFormatKHR {
                .format {vk::Format::eB8G8R8A8Srgb},
                .colorSpace {vk::ColorSpaceKHR::eSrgbNonlinear}};
        static constexpr vk::Format DepthFormat = vk::Format::eD32Sfloat;
    public:
        Renderer();
        ~Renderer() noexcept;

        Renderer(const Renderer&)             = delete;
        Renderer(Renderer&&)                  = delete;
        Renderer& operator= (const Renderer&) = delete;
        Renderer& operator= (Renderer&&)      = delete;

        // Returns true if a resize occurred
        bool recordOnThread(
            std::function<
                // Flying Frame Idx, command buffer, swapchain idx, swapchain
                void(vk::CommandBuffer, U32, vulkan::Swapchain&)>) const;
        [[nodiscard]] bool                  shouldWindowClose() const noexcept;
        // lives as long as the renderer
        [[nodiscard]] const vulkan::Device* getDevice() const noexcept;

    private:
        struct RenderingCriticalSection
        {
            std::unique_ptr<vulkan::FrameManager> frame_manager;
            std::unique_ptr<vulkan::Swapchain>    swapchain;
        };

        static std::unique_ptr<RenderingCriticalSection> makeCriticalSection(
            const vulkan::Device&, vk::SurfaceKHR, const Window&);

        std::unique_ptr<Window> window;

        std::unique_ptr<vulkan::Instance>  instance;
        vk::UniqueSurfaceKHR               surface;
        std::unique_ptr<vulkan::Device>    device;
        std::unique_ptr<vulkan::Allocator> allocator;

        util::Mutex<std::unique_ptr<RenderingCriticalSection>> critical_section;
    };
} // namespace gfx
