#include "frame_manager.hpp"
#include "device.hpp"
#include <__expected/unexpected.h>
#include <optional>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::vulkan
{
    Frame::Frame(const Device& device_, vk::SwapchainKHR swapchain_)
        : device {&device_}
        , swapchain {swapchain_}
    {
        const vk::SemaphoreCreateInfo semaphoreCreateInfo {
            .sType {vk::StructureType::eSemaphoreCreateInfo},
            .pNext {nullptr},
            .flags {}};

        const vk::FenceCreateInfo fenceCreateInfo {
            .sType {vk::StructureType::eFenceCreateInfo},
            .pNext {nullptr},
            .flags {vk::FenceCreateFlagBits::eSignaled},
        };

        const vk::CommandPoolCreateInfo commandPoolCreateInfo {
            .sType {vk::StructureType::eCommandPoolCreateInfo},
            .pNext {nullptr},
            .flags {vk::CommandPoolCreateFlagBits::eTransient},
            .queueFamilyIndex {
                device_ // NOLINT
                    .getFamilyOfQueueType(Device::QueueType::Graphics)
                    .value()},
        };

        this->command_pool = this->device->getDevice().createCommandPoolUnique(
            commandPoolCreateInfo);

        const vk::CommandBufferAllocateInfo commandBufferAllocateInfo {
            .sType {vk::StructureType::eCommandBufferAllocateInfo},
            .pNext {nullptr},
            .commandPool {*this->command_pool},
            .level {vk::CommandBufferLevel::ePrimary},
            .commandBufferCount {1},
        };

        this->command_buffer = std::move(
            this->device->getDevice()
                .allocateCommandBuffersUnique(commandBufferAllocateInfo)
                .at(0));

        this->image_available = this->device->getDevice().createSemaphoreUnique(
            semaphoreCreateInfo);
        this->render_finished = this->device->getDevice().createSemaphoreUnique(
            semaphoreCreateInfo);
        this->frame_in_flight =
            this->device->getDevice().createFenceUnique(fenceCreateInfo);
    }

    std::expected<void, Frame::ResizeNeeded> Frame::recordAndDisplay(
        std::optional<vk::Fence>                    previousFrameFence,
        std::function<void(vk::CommandBuffer, U32)> withCommandBuffer)
    {
        const auto [result, maybeNextImageIdx] =
            this->device->getDevice().acquireNextImageKHR(
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

        bool shouldResize = false;

        this->device->acquireQueue(
            Device::QueueType::Graphics,
            [&](vk::Queue queue)
            {
                const vk::Device device = this->device->getDevice();

                device.resetFences(*this->frame_in_flight);
                device.resetCommandPool(*this->command_pool);

                const vk::CommandBufferBeginInfo commandBufferBeginInfo {
                    .sType {vk::StructureType::eCommandBufferBeginInfo},
                    .pNext {nullptr},
                    .flags {vk::CommandBufferUsageFlagBits::eOneTimeSubmit},
                    .pInheritanceInfo {nullptr},
                };

                this->command_buffer->begin(commandBufferBeginInfo);

                withCommandBuffer(*this->command_buffer, maybeNextImageIdx);

                this->command_buffer->end();

                const vk::PipelineStageFlags dstStageWaitFlags =
                    vk::PipelineStageFlagBits::eColorAttachmentOutput;

                const vk::SubmitInfo queueSubmitInfo {
                    .sType {vk::StructureType::eSubmitInfo},
                    .pNext {nullptr},
                    .waitSemaphoreCount {1},
                    .pWaitSemaphores {&*this->image_available},
                    .pWaitDstStageMask {&dstStageWaitFlags},
                    .commandBufferCount {1},
                    .pCommandBuffers {&*this->command_buffer},
                    .signalSemaphoreCount {1},
                    .pSignalSemaphores {&*this->render_finished},
                };

                queue.submit(queueSubmitInfo);

                if (previousFrameFence.has_value())
                {
                    std::ignore = device.waitForFences(
                        *previousFrameFence, vk::True, TimeoutNs);
                }

                const vk::PresentInfoKHR presentInfo {
                    .sType {vk::StructureType::ePresentInfoKHR},
                    .pNext {nullptr},
                    .waitSemaphoreCount {1},
                    .pWaitSemaphores {&*this->render_finished},
                    .swapchainCount {1},
                    .pSwapchains {&this->swapchain},
                    .pImageIndices {&maybeNextImageIdx},
                    .pResults {nullptr},
                };

                const vk::Result presentResult = queue.presentKHR(presentInfo);

                switch (result) // NOLINT
                {
                case vk::Result::eSuccess:
                    break;
                case vk::Result::eSuboptimalKHR:
                    [[fallthrough]];
                case vk::Result::eErrorOutOfDateKHR:
                    shouldResize = true;
                default:
                    util::panic(
                        "acquireNextImage returned {}", vk::to_string(result));
                }
            });

        if (shouldResize)
        {
            return std::unexpected(Frame::ResizeNeeded {});
        }
        else
        {
            return {};
        }
    }

    vk::Fence Frame::getFrameInFlightFence() const noexcept
    {
        return *this->frame_in_flight;
    }

    FrameManager::FrameManager(const Device& device, vk::SwapchainKHR swapchain)
        : previous_frame_finished_fence {std::nullopt}
        , flying_frames {Frame {device, swapchain}, Frame {device, swapchain}, Frame {device, swapchain}}
        , flying_frame_index {0}
    {}

    std::expected<void, Frame::ResizeNeeded> FrameManager::recordAndDisplay(
        std::function<void(std::size_t, vk::CommandBuffer, U32)> recordFunc)
    {
        this->flying_frame_index += 1;
        this->flying_frame_index %= FramesInFlight;

        auto result =
            this->flying_frames.at(this->flying_frame_index)
                .recordAndDisplay(
                    this->previous_frame_finished_fence,
                    [&](vk::CommandBuffer commandBuffer, U32 swapchainIndex)
                    {
                        recordFunc(
                            this->flying_frame_index,
                            commandBuffer,
                            swapchainIndex);
                    });

        this->previous_frame_finished_fence =
            this->flying_frames.at(this->flying_frame_index)
                .getFrameInFlightFence();

        return result;
    }
} // namespace gfx::vulkan
