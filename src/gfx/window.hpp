#pragma once

#include <span>

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

        [[nodiscard]] bool shouldWindowClose();
        void               beginFrame();

    private:
        GLFWwindow* window;
    };

} // namespace gfx
