#pragma once

#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::vulkan
{
    class Device;

    class Swapchain
    {
    public:
        Swapchain(const Device&, vk::SurfaceKHR, vk::Extent2D);
        ~Swapchain() noexcept = default;

        Swapchain(const Swapchain&)             = delete;
        Swapchain(Swapchain&&)                  = delete;
        Swapchain& operator= (const Swapchain&) = delete;
        Swapchain& operator= (Swapchain&&)      = delete;

        [[nodiscard]] std::span<const vk::ImageView> getViews() const noexcept;
        [[nodiscard]] std::span<const vk::Image>     getImages() const noexcept;
        [[nodiscard]] vk::Extent2D                   getExtent() const noexcept;
        [[nodiscard]] vk::SwapchainKHR operator* () const noexcept;

    private:
        std::vector<vk::UniqueImageView> image_views;
        std::vector<vk::ImageView>       dense_image_views;
        std::vector<vk::Image>           images;
        vk::UniqueSwapchainKHR           swapchain;
        vk::Extent2D                     extent;
    };
} // namespace gfx::vulkan
