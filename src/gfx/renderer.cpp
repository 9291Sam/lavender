#include "renderer.hpp"
#include "util/log.hpp"
#include "vulkan/device.hpp"
#include "vulkan/instance.hpp"
#include "window.hpp"
#include <memory>

namespace gfx
{
    Renderer::Renderer()
    {
        this->window   = std::make_unique<Window>();
        this->instance = std::make_unique<vulkan::Instance>();
        this->surface  = this->window->createSurface(**this->instance);
        this->device =
            std::make_unique<vulkan::Device>(**this->instance, *this->surface);
    }

    Renderer::~Renderer() noexcept = default;

    void Renderer::renderOnThread()
    {
        while (!this->window->shouldWindowClose())
        {
            this->window->beginFrame();
        }
    }
} // namespace gfx
