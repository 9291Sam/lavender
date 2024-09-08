#include "renderer.hpp"
#include "window.hpp"

namespace gfx
{

    Renderer::Renderer()
        : window {std::make_unique<Window>()}
    {}

    Renderer::~Renderer() noexcept = default;

    void Renderer::renderOnThread()
    {
        while (!this->window->shouldWindowClose())
        {
            this->window->beginFrame();
        }
    }
} // namespace gfx
