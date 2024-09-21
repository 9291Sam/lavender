#include "image.hpp"
#include "allocator.hpp"
#include <util/log.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::vulkan
{
    Image2D::Image2D( // NOLINT: this->memory is initalized via vmaCreateImage
        const Allocator*        allocator_,
        vk::Device              device,
        vk::Extent2D            extent_,
        vk::Format              format_,
        vk::ImageLayout         layout,
        vk::ImageUsageFlags     usage,
        vk::ImageAspectFlags    aspect_,
        vk::ImageTiling         tiling,
        vk::MemoryPropertyFlags memoryPropertyFlags)
        : allocator {**allocator_}
        , extent {extent_}
        , format {format_}
        , aspect {aspect_}
        , image {nullptr}
        , memory {nullptr}
        , view {nullptr}
    {
        const VkImageCreateInfo imageCreateInfo {
            .sType {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
            .pNext {nullptr},
            .flags {},
            .imageType {VK_IMAGE_TYPE_2D},
            .format {static_cast<VkFormat>(format)},
            .extent {VkExtent3D {
                .width {this->extent.width},
                .height {this->extent.height},
                .depth {1}}},
            .mipLevels {1},
            .arrayLayers {1},
            .samples {VK_SAMPLE_COUNT_1_BIT},
            .tiling {static_cast<VkImageTiling>(tiling)},
            .usage {static_cast<VkImageUsageFlags>(usage)},
            .sharingMode {VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount {0},
            .pQueueFamilyIndices {nullptr},
            .initialLayout {static_cast<VkImageLayout>(layout)}};

        const VmaAllocationCreateInfo imageAllocationCreateInfo {
            .flags {},
            .usage {VMA_MEMORY_USAGE_AUTO},
            .requiredFlags {
                static_cast<VkMemoryPropertyFlags>(memoryPropertyFlags)},
            .preferredFlags {},
            .memoryTypeBits {},
            .pool {nullptr},
            .pUserData {nullptr},
            .priority {1.0f}};

        VkImage outputImage = nullptr;

        const vk::Result result {::vmaCreateImage(
            this->allocator,
            &imageCreateInfo,
            &imageAllocationCreateInfo,
            &outputImage,
            &this->memory,
            nullptr)};

        util::assertFatal(
            result == vk::Result::eSuccess,
            "Failed to allocate image memory {}",
            vk::to_string(result));

        util::assertFatal(
            outputImage != nullptr, "Returned image was nullptr!");

        this->image = vk::Image {outputImage};

        vk::ImageViewCreateInfo imageViewCreateInfo {
            .sType {vk::StructureType::eImageViewCreateInfo},
            .pNext {nullptr},
            .flags {},
            .image {this->image},
            .viewType {vk::ImageViewType::e2D},
            .format {this->format},
            .components {},
            .subresourceRange {vk::ImageSubresourceRange {
                .aspectMask {this->aspect},
                .baseMipLevel {0},
                .levelCount {1},
                .baseArrayLayer {0},
                .layerCount {1},
            }},
        };

        this->view = device.createImageViewUnique(imageViewCreateInfo);

        // if (engine::getSettings()
        //         .lookupSetting<engine::Setting::EnableGFXValidation>())
        // {
        //     {
        //         vk::DebugUtilsObjectNameInfoEXT nameSetInfo {
        //             .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
        //             .pNext {nullptr},
        //             .objectType {vk::ObjectType::eImage},
        //             .objectHandle
        //             {std::bit_cast<std::uint64_t>(this->image)}, .pObjectName
        //             {name.c_str()},
        //         };

        //         device.setDebugUtilsObjectNameEXT(nameSetInfo);
        //     }

        //     {
        //         vk::DebugUtilsObjectNameInfoEXT nameSetInfo {
        //             .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
        //             .pNext {nullptr},
        //             .objectType {vk::ObjectType::eImageView},
        //             .objectHandle
        //             {std::bit_cast<std::uint64_t>(*this->view)}, .pObjectName
        //             {name.c_str()},
        //         };

        //         device.setDebugUtilsObjectNameEXT(nameSetInfo);
        //     }
        // }
    }

    Image2D::~Image2D()
    {
        this->free();
    }

    Image2D::Image2D(Image2D&& other) noexcept
        : allocator {other.allocator}
        , extent {other.extent}
        , format {other.format}
        , aspect {other.aspect}
        , image {other.image}
        , memory {other.memory}
        , view {std::move(other.view)}
    {
        other.allocator = nullptr;
        other.extent    = {};
        other.format    = {};
        other.aspect    = {};
        other.image     = nullptr;
        other.memory    = nullptr;
    }

    Image2D& Image2D::operator= (Image2D&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->~Image2D();

        new (this) Image2D {std::move(other)};

        return *this;
    }

    vk::Image Image2D::operator* () const
    {
        return this->image;
    }

    vk::ImageView Image2D::getView() const
    {
        return *this->view;
    }

    vk::Format Image2D::getFormat() const
    {
        return this->format;
    }

    vk::Extent2D Image2D::getExtent() const
    {
        return this->extent;
    }

    void Image2D::free()
    {
        if (this->allocator != nullptr)
        {
            vmaDestroyImage(
                this->allocator,
                static_cast<VkImage>(this->image),
                this->memory);
        }
    }

} // namespace gfx::vulkan
