#include "swapchain.hpp"
#include "device.hpp"
#include "frame_manager.hpp"
#include "gfx/renderer.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    Swapchain::Swapchain(const Device& device, vk::SurfaceKHR surface, vk::Extent2D extent_)
        : extent {extent_}
    {
        const std::vector<vk::SurfaceFormatKHR> availableSurfaceFormats =
            device.getPhysicalDevice().getSurfaceFormatsKHR(surface);

        util::assertFatal(
            std::ranges::find(availableSurfaceFormats, ::gfx::Renderer::ColorFormat)
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

        util::logLog(
            "Selected {} as present mode with inital size of {}x{}",
            vk::to_string(selectedPresentMode),
            this->extent.width,
            this->extent.height);

        const vk::SurfaceCapabilitiesKHR surfaceCapabilities =
            device.getPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
        const u32 numberOfSwapchainImages = std::min(
            {std::max({4U, surfaceCapabilities.minImageCount}), surfaceCapabilities.maxImageCount});

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

        if constexpr (util::isDebugBuild())
        {
            device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eSwapchainKHR},
                .objectHandle {std::bit_cast<u64>(*this->swapchain)},
                .pObjectName {"Swapchain"},
            });
        }

        this->image_views.reserve(this->images.size());
        this->dense_image_views.reserve(this->images.size());

        std::size_t idx = 0;
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

            vk::UniqueImageView imageView =
                device->createImageViewUnique(swapchainImageViewCreateInfo);

            if constexpr (util::isDebugBuild())
            {
                std::string imageName = std::format("Swapchain Image #{}", idx);
                std::string viewName  = std::format("View #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eImage},
                    .objectHandle {std::bit_cast<u64>(i)},
                    .pObjectName {imageName.c_str()},
                });

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eImageView},
                    .objectHandle {std::bit_cast<u64>(*imageView)},
                    .pObjectName {viewName.c_str()},
                });
            }

            this->dense_image_views.push_back(*imageView);
            this->image_views.push_back(std::move(imageView));

            idx += 1;
        }

        util::logTrace("Created swapchain");
    }

    std::span<const vk::ImageView> Swapchain::getViews() const noexcept
    {
        return this->dense_image_views;
    }

    std::span<const vk::Image> Swapchain::getImages() const noexcept
    {
        return this->images;
    }

    vk::Extent2D Swapchain::getExtent() const noexcept
    {
        return this->extent;
    }

    vk::SwapchainKHR Swapchain::operator* () const noexcept
    {
        return *this->swapchain;
    }

} // namespace gfx::vulkan
