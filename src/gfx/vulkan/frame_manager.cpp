#include "frame_manager.hpp"
#include "device.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/frame_manager.hpp"
#include <expected>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::vulkan
{
    Frame::Frame(const Device& device_, vk::SwapchainKHR swapchain_, std::size_t number)
        : device {&device_}
        , swapchain {swapchain_}
    {
        const vk::SemaphoreCreateInfo semaphoreCreateInfo {
            .sType {vk::StructureType::eSemaphoreCreateInfo}, .pNext {nullptr}, .flags {}};

        const vk::FenceCreateInfo fenceCreateInfo {
            .sType {vk::StructureType::eFenceCreateInfo},
            .pNext {nullptr},
            .flags {vk::FenceCreateFlagBits::eSignaled},
        };

        const vk::CommandPoolCreateInfo commandPoolCreateInfo {
            .sType {vk::StructureType::eCommandPoolCreateInfo},
            .pNext {nullptr},
            .flags {
                vk::CommandPoolCreateFlagBits::eTransient
                | vk::CommandPoolCreateFlagBits::eResetCommandBuffer},
            .queueFamilyIndex {device_ // NOLINT
                                   .getFamilyOfQueueType(Device::QueueType::Graphics)
                                   .value()},
        };

        const std::string commandPoolName = std::format("Frame #{} Command Pool", number);
        const std::string imageAvailableName =
            std::format("Frame #{} Image Available Semaphore", number);
        const std::string renderFinishedName =
            std::format("Frame #{} Render Finished Semaphore", number);
        const std::string frameInFlightName =
            std::format("Frame #{} Frame In Flight Fence", number);

        this->command_pool =
            this->device->getDevice().createCommandPoolUnique(commandPoolCreateInfo);

        this->image_available =
            this->device->getDevice().createSemaphoreUnique(semaphoreCreateInfo);
        this->render_finished =
            this->device->getDevice().createSemaphoreUnique(semaphoreCreateInfo);
        this->frame_in_flight = this->device->getDevice().createFenceUnique(fenceCreateInfo);

        if constexpr (util::isDebugBuild())
        {
            device->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eCommandPool},
                .objectHandle {std::bit_cast<u64>(*this->command_pool)},
                .pObjectName {commandPoolName.c_str()},
            });

            device->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eSemaphore},
                .objectHandle {std::bit_cast<u64>(*this->image_available)},
                .pObjectName {imageAvailableName.c_str()},
            });

            device->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eSemaphore},
                .objectHandle {std::bit_cast<u64>(*this->render_finished)},
                .pObjectName {renderFinishedName.c_str()},
            });

            device->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eFence},
                .objectHandle {std::bit_cast<u64>(*this->frame_in_flight)},
                .pObjectName {frameInFlightName.c_str()},
            });
        }
    }

    Frame::~Frame() noexcept
    {
        this->device->getDevice().waitIdle();
    }

    std::expected<void, Frame::ResizeNeeded> Frame::recordAndDisplay(
        std::optional<vk::Fence>                    previousFrameFence,
        std::function<void(vk::CommandBuffer, u32)> withCommandBuffer,
        const BufferStager&                         stager)
    {
        const auto [acquireImageResult, maybeNextImageIdx] =
            this->device->getDevice().acquireNextImageKHR(
                this->swapchain, TimeoutNs, *this->image_available, nullptr);

        switch (acquireImageResult) // NOLINT
        {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eSuboptimalKHR:
            [[fallthrough]];
        case vk::Result::eErrorOutOfDateKHR:
            return std::unexpected(Frame::ResizeNeeded {});
        default:
            util::panic("acquireNextImage returned {}", vk::to_string(acquireImageResult));
        }

        bool shouldResize = false;

        this->device->acquireQueue(
            Device::QueueType::Graphics,
            [&](vk::Queue queue)
            {
                this->device->getDevice().resetFences(*this->frame_in_flight);
                this->device->getDevice().resetCommandPool(*this->command_pool);

                // HACK: on nvidia, there's a driver bug where calling
                // vkResetCommandPool doesn't actually free the resources of its
                // underlying command buffers, we can skirt around this by
                // having an explicit call to free and reallocate the command
                // buffer every frame.
                this->command_buffer.reset();

                const vk::CommandBufferAllocateInfo commandBufferAllocateInfo {
                    .sType {vk::StructureType::eCommandBufferAllocateInfo},
                    .pNext {nullptr},
                    .commandPool {*this->command_pool},
                    .level {vk::CommandBufferLevel::ePrimary},
                    .commandBufferCount {1},
                };

                this->command_buffer =
                    std::move(this->device->getDevice()
                                  .allocateCommandBuffersUnique(commandBufferAllocateInfo)
                                  .at(0));

                const vk::CommandBufferBeginInfo commandBufferBeginInfo {
                    .sType {vk::StructureType::eCommandBufferBeginInfo},
                    .pNext {nullptr},
                    .flags {vk::CommandBufferUsageFlagBits::eOneTimeSubmit},
                    .pInheritanceInfo {nullptr},
                };

                this->command_buffer->reset();
                this->command_buffer->begin(commandBufferBeginInfo);

                // HACK: flush all buffers on this
                stager.flushTransfers(*this->command_buffer, *this->frame_in_flight);

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

                queue.submit(queueSubmitInfo, *this->frame_in_flight);

                if (previousFrameFence.has_value())
                {
                    std::ignore = this->device->getDevice().waitForFences(
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

                const vk::Result presentResult =
                    vk::Result {vk::defaultDispatchLoaderDynamic.vkQueuePresentKHR(
                        queue,
                        // NOLINTNEXTLINE
                        reinterpret_cast<const VkPresentInfoKHR*>(&presentInfo))};

                switch (presentResult) // NOLINT
                {
                case vk::Result::eSuccess:
                    break;
                case vk::Result::eSuboptimalKHR:
                    [[fallthrough]];
                case vk::Result::eErrorOutOfDateKHR:
                    shouldResize = true;
                    break;
                default:
                    util::panic("acquireNextImage returned {}", vk::to_string(presentResult));
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

    FrameManager::FrameManager(const Device& device_, vk::SwapchainKHR swapchain)
        : device {device_.getDevice()}
        , previous_frame_finished_fence {std::nullopt}
        , flying_frames {Frame {device_, swapchain, 0}, Frame {device_, swapchain, 1}, Frame {device_, swapchain, 2}}
        , flying_frame_index {0}
    {}

    FrameManager::~FrameManager() noexcept = default;

    std::expected<void, Frame::ResizeNeeded> FrameManager::recordAndDisplay(
        std::function<void(std::size_t, vk::CommandBuffer, u32)> recordFunc,
        const BufferStager&                                      stager)
    {
        this->flying_frame_index += 1;
        this->flying_frame_index %= FramesInFlight;

        auto result =
            this->flying_frames.at(this->flying_frame_index)
                .recordAndDisplay(
                    this->previous_frame_finished_fence,
                    [&](vk::CommandBuffer commandBuffer, u32 swapchainIndex)
                    {
                        recordFunc(this->flying_frame_index, commandBuffer, swapchainIndex);
                    },
                    stager);

        this->previous_frame_finished_fence =
            this->flying_frames.at(this->flying_frame_index).getFrameInFlightFence();

        return result;
    }
} // namespace gfx::vulkan
