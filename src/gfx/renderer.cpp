#include "renderer.hpp"
#include "instance.hpp"
#include "window.hpp"

namespace gfx
{

    Renderer::Renderer()
        : window {std::make_unique<Window>()}
        , instance {std::make_unique<Instance>()}
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
