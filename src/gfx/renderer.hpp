#pragma once

#include "util/misc.hpp"
#include "vulkan/buffer.hpp"
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
        static constexpr vk::SurfaceFormatKHR ColorFormat = vk::SurfaceFormatKHR {
            .format {vk::Format::eB8G8R8A8Srgb}, .colorSpace {vk::ColorSpaceKHR::eSrgbNonlinear}};
        static constexpr vk::Format DepthFormat = vk::Format::eD32Sfloat;
    public:
        Renderer();
        ~Renderer() noexcept;

        Renderer(const Renderer&)             = delete;
        Renderer(Renderer&&)                  = delete;
        Renderer& operator= (const Renderer&) = delete;
        Renderer& operator= (Renderer&&)      = delete;

        // Returns true if a resize occurred
        // Command buffer, swapchain idx, swapchain, frame idx
        bool recordOnThread(
            std::function<void(vk::CommandBuffer, u32, vulkan::Swapchain&, std::size_t)>) const;
        [[nodiscard]] bool shouldWindowClose() const noexcept;

        [[nodiscard]] const vulkan::Device*       getDevice() const noexcept;
        [[nodiscard]] const vulkan::Allocator*    getAllocator() const noexcept;
        [[nodiscard]] const Window*               getWindow() const noexcept;
        [[nodiscard]] const vulkan::BufferStager& getStager() const noexcept;
        [[nodiscard]] u32                         getFrameNumber() const noexcept;
        [[nodiscard]] float                       getTimeAlive() const noexcept;

    private:
        struct RenderingCriticalSection
        {
            std::unique_ptr<vulkan::FrameManager> frame_manager;
            std::unique_ptr<vulkan::Swapchain>    swapchain;
        };

        static std::unique_ptr<RenderingCriticalSection>
        makeCriticalSection(const vulkan::Device&, vk::SurfaceKHR, const Window&);

        std::unique_ptr<Window> window;

        std::unique_ptr<vulkan::Instance>     instance;
        vk::UniqueSurfaceKHR                  surface;
        std::unique_ptr<vulkan::Device>       device;
        std::unique_ptr<vulkan::Allocator>    allocator;
        std::unique_ptr<vulkan::BufferStager> stager;

        util::Mutex<std::unique_ptr<RenderingCriticalSection>> critical_section;

        mutable std::atomic<float> time_alive;
        mutable std::atomic<u32>   frame_number;
    };
} // namespace gfx
