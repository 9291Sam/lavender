#pragma once

#include <memory>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx
{
    class Window;

    namespace vulkan
    {
        class Instance;
        class Device;
    } // namespace vulkan

    class Renderer
    {
    public:
        Renderer();
        ~Renderer() noexcept;

        Renderer(const Renderer&)             = delete;
        Renderer(Renderer&&)                  = delete;
        Renderer& operator= (const Renderer&) = delete;
        Renderer& operator= (Renderer&&)      = delete;

        void renderOnThread();

    private:
        std::unique_ptr<Window>           window;
        std::unique_ptr<vulkan::Instance> instance;
        vk::UniqueSurfaceKHR              surface;
        std::unique_ptr<vulkan::Device>   device;
    };
} // namespace gfx
