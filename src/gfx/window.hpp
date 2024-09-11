#pragma once

#include <span>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

struct GLFWwindow;

namespace gfx
{
    class Window
    {
    public:
        Window();
        ~Window() noexcept;

        Window(const Window&)             = delete;
        Window(Window&&)                  = delete;
        Window& operator= (const Window&) = delete;
        Window& operator= (Window&&)      = delete;

        [[nodiscard]] static std::span<const char*> getRequiredExtensions();
        [[nodiscard]] vk::UniqueSurfaceKHR          createSurface(vk::Instance);

        [[nodiscard]] bool shouldWindowClose();
        void               beginFrame();

    private:
        GLFWwindow* window;
    };

} // namespace gfx
