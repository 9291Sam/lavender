#include "frame_manager.hpp"
#include "device.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::vulkan
{
    Frame::Frame(const Device& device_, vk::SwapchainKHR swapchain_)
        : device {device_.getDevice()}
        , swapchain {swapchain_}
    {
        const vk::SemaphoreCreateInfo semaphoreCreateInfo {
            .sType {vk::StructureType::eSemaphoreCreateInfo},
            .pNext {nullptr},
            .flags {}};

        const vk::FenceCreateInfo fenceCreateInfo {
            .sType {vk::StructureType::eFenceCreateInfo},
            .pNext {nullptr},
            .flags {},
        };

        const vk::CommandPoolCreateInfo commandPoolCreateInfo {
            .sType {vk::StructureType::eCommandPoolCreateInfo},
            .pNext {nullptr},
            .flags {vk::CommandPoolCreateFlagBits::eTransient},
            .queueFamilyIndex {
                device_.getFamilyOfQueueType(Device::QueueType::Graphics)
                    .value()},
        };

        this->command_pool =
            this->device.createCommandPoolUnique(commandPoolCreateInfo);

        const vk::CommandBufferAllocateInfo commandBufferAllocateInfo {
            .sType {vk::StructureType::eCommandBufferAllocateInfo},
            .pNext {nullptr},
            .commandPool {*this->command_pool},
            .level {vk::CommandBufferLevel::ePrimary},
            .commandBufferCount {1},
        };

        this->command_buffer = std::move(
            this->device.allocateCommandBuffersUnique(commandBufferAllocateInfo)
                .at(0));

        this->image_available =
            this->device.createSemaphoreUnique(semaphoreCreateInfo);
        this->render_finished =
            this->device.createSemaphoreUnique(semaphoreCreateInfo);
        this->frame_in_flight = this->device.createFenceUnique(fenceCreateInfo);
    }

    std::expected<void, Frame::ResizeNeeded> Frame::recordAndDisplay(
        std::optional<vk::Fence>                    previousFrameFence,
        std::function<void(vk::CommandBuffer, U32)> withCommandBuffer)
    {
        const auto [result, maybeNextImageIdx] =
            this->device.acquireNextImageKHR(
                this->swapchain, TimeoutNs, *this->image_available, nullptr);

        switch (result) // NOLINT
        {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eSuboptimalKHR:
            [[fallthrough]];
        case vk::Result::eErrorOutOfDateKHR:
            return std::unexpected(Frame::ResizeNeeded {});
        default:
            util::panic("acquireNextImage returned {}", vk::to_string(result));
        }
    }
} // namespace gfx::vulkan
