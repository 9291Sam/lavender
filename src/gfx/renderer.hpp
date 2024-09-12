#pragma once

#include <functional>
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

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
        static constexpr vk::SurfaceFormatKHR ColorFormat =
            vk::SurfaceFormatKHR {
                .format {vk::Format::eR8G8B8A8Srgb},
                .colorSpace {vk::ColorSpaceKHR::eSrgbNonlinear}};
        static constexpr vk::Format DepthFormat = vk::Format::eD32Sfloat;
    public:
        Renderer();
        ~Renderer() noexcept;

        Renderer(const Renderer&)             = delete;
        Renderer(Renderer&&)                  = delete;
        Renderer& operator= (const Renderer&) = delete;
        Renderer& operator= (Renderer&&)      = delete;

        void recordOnThread(std::function<void(vk::CommandBuffer)>) const;
        [[nodiscard]] bool shouldWindowClose() const noexcept;

    private:
        std::unique_ptr<Window>           window;
        std::unique_ptr<vulkan::Instance> instance;
        vk::UniqueSurfaceKHR              surface;
        std::unique_ptr<vulkan::Device>   device;
    };
} // namespace gfx
