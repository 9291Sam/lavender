#pragma once

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

        void beginFrame();
        bool shouldWindowClose();
    private:
        GLFWwindow* window;
    };

} // namespace gfx
