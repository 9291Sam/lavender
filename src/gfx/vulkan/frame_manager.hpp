#pragma once

#include "device.hpp"
#include <chrono>
#include <cstddef>
#include <functional>
#include <util/misc.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::vulkan
{
    class Device;

    constexpr std::size_t FramesInFlight = 3;
    constexpr std::size_t TimeoutNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<std::size_t> {5})
            .count();

    class Frame
    {
    public:
        struct ResizeNeeded
        {};
    public:
        Frame(const Device&, vk::SwapchainKHR, std::size_t number);
        ~Frame() noexcept;

        Frame(const Frame&)             = delete;
        Frame(Frame&&)                  = delete;
        Frame& operator= (const Frame&) = delete;
        Frame& operator= (Frame&&)      = delete;

        [[nodiscard]] std::expected<void, Frame::ResizeNeeded> recordAndDisplay(
            std::optional<vk::Fence> previousFrameFence,
            // u32 is the swapchain image's index
            std::function<void(vk::CommandBuffer, u32)>);

        [[nodiscard]] vk::Fence getFrameInFlightFence() const noexcept;

    private:
        vk::UniqueSemaphore image_available;
        vk::UniqueSemaphore render_finished;
        vk::UniqueFence     frame_in_flight;

        vk::UniqueCommandPool   command_pool;
        vk::UniqueCommandBuffer command_buffer;

        const vulkan::Device* device;
        vk::SwapchainKHR      swapchain;
    };

    class FrameManager
    {
    public:

        FrameManager(const Device&, vk::SwapchainKHR);
        ~FrameManager() noexcept;

        FrameManager(const FrameManager&)             = delete;
        FrameManager(FrameManager&&)                  = delete;
        FrameManager& operator= (const FrameManager&) = delete;
        FrameManager& operator= (FrameManager&&)      = delete;

        // std::size_t is the flying frame index
        // u32 is the swapchain image's index
        [[nodiscard]] std::expected<void, Frame::ResizeNeeded> recordAndDisplay(
            std::function<void(std::size_t, vk::CommandBuffer, u32)>);
    private:
        vk::Device                        device;
        std::optional<vk::Fence>          previous_frame_finished_fence;
        std::array<Frame, FramesInFlight> flying_frames;
        std::size_t                       flying_frame_index;
    };

} // namespace gfx::vulkan
