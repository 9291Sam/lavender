#pragma once

#include <memory>

namespace gfx
{
    class Window;
    class Instance;

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
        std::unique_ptr<Window>   window;
        std::unique_ptr<Instance> instance;
    };
} // namespace gfx
