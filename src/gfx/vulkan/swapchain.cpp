#include "swapchain.hpp"
#include "device.hpp"
#include "gfx/renderer.hpp"
#include "util/log.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    Swapchain::Swapchain(
        const Device& device, vk::SurfaceKHR surface, vk::Extent2D extent_)
        : extent {extent_}
    {
        const std::vector<vk::SurfaceFormatKHR> availableSurfaceFormats =
            device.getPhysicalDevice().getSurfaceFormatsKHR(surface);

        util::assertFatal(
            std::ranges::find(
                availableSurfaceFormats, ::gfx::Renderer::ColorFormat)
                != availableSurfaceFormats.cend(),
            "Required surface format {} {} is not supported!",
            vk::to_string(::gfx::Renderer::ColorFormat.format),
            vk::to_string(::gfx::Renderer::ColorFormat.colorSpace));

        const std::array desiredPresentModes {
            vk::PresentModeKHR::eMailbox,
            vk::PresentModeKHR::eImmediate,
            vk::PresentModeKHR::eFifo,
        };

        const std::vector<vk::PresentModeKHR> availablePresentModes =
            device.getPhysicalDevice().getSurfacePresentModesKHR(surface);

        vk::PresentModeKHR selectedPresentMode = vk::PresentModeKHR::eFifo;

        for (vk::PresentModeKHR desiredPresentMode : desiredPresentModes)
        {
            if (std::ranges::find(availablePresentModes, desiredPresentMode)
                != availablePresentModes.cend())
            {
                selectedPresentMode = desiredPresentMode;
                break;
            }
        }

        util::logTrace(
            "Selected {} as present mode with inital size of {}x{}",
            vk::to_string(selectedPresentMode),
            this->extent.width,
            this->extent.height);

        const vk::SurfaceCapabilitiesKHR surfaceCapabilities =
            device.getPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
        const U32 numberOfSwapchainImages =
            surfaceCapabilities.maxImageCount == 0
                ? surfaceCapabilities.minImageCount + 1
                : surfaceCapabilities.maxImageCount;

        const vk::SwapchainCreateInfoKHR swapchainCreateInfo {
            .sType {vk::StructureType::eSwapchainCreateInfoKHR},
            .pNext {nullptr},
            .flags {},
            .surface {surface},
            .minImageCount {numberOfSwapchainImages},
            .imageFormat {gfx::Renderer::ColorFormat.format},
            .imageColorSpace {gfx::Renderer::ColorFormat.colorSpace},
            .imageExtent {this->extent},
            .imageArrayLayers {1},
            .imageUsage {vk::ImageUsageFlagBits::eColorAttachment},
            .imageSharingMode {vk::SharingMode::eExclusive},
            .queueFamilyIndexCount {0},
            .pQueueFamilyIndices {nullptr},
            .preTransform {surfaceCapabilities.currentTransform},
            .compositeAlpha {vk::CompositeAlphaFlagBitsKHR::eOpaque},
            .presentMode {selectedPresentMode},
            .clipped {vk::True},
            .oldSwapchain {nullptr},
        };

        this->swapchain = device->createSwapchainKHRUnique(swapchainCreateInfo);
        this->images    = device->getSwapchainImagesKHR(*this->swapchain);

        this->image_views.reserve(this->images.size());

        for (vk::Image i : this->images)
        {
            const vk::ImageViewCreateInfo swapchainImageViewCreateInfo {
                .sType {vk::StructureType::eImageViewCreateInfo},
                .pNext {nullptr},
                .flags {},
                .image {i},
                .viewType {vk::ImageViewType::e2D},
                .format {gfx::Renderer::ColorFormat.format},
                .components {vk::ComponentMapping {
                    .r {vk::ComponentSwizzle::eIdentity},
                    .g {vk::ComponentSwizzle::eIdentity},
                    .b {vk::ComponentSwizzle::eIdentity},
                    .a {vk::ComponentSwizzle::eIdentity},
                }},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eColor},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            };

            this->image_views.push_back(
                device->createImageViewUnique(swapchainImageViewCreateInfo));
        }
    }
} // namespace gfx::vulkan