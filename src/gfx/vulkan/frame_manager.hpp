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
        Frame(const Device&, vk::SwapchainKHR);
        ~Frame() = default;

        Frame(const Frame&)             = delete;
        Frame(Frame&&)                  = delete;
        Frame& operator= (const Frame&) = delete;
        Frame& operator= (Frame&&)      = delete;

        std::expected<void, Frame::ResizeNeeded> recordAndDisplay(
            std::optional<vk::Fence> previousFrameFence,
            // U32 is the swapchain image's index
            std::function<void(vk::CommandBuffer, U32)>);

    private:
        vk::UniqueSemaphore image_available;
        vk::UniqueSemaphore render_finished;
        vk::UniqueFence     frame_in_flight;

        vk::UniqueCommandBuffer command_buffer;
        vk::UniqueCommandPool   command_pool;

        vk::Device       device;
        vk::SwapchainKHR swapchain;
    };

    class FrameManager
    {
    public:

        FrameManager(vk::Instance, vk::SurfaceKHR);
        ~FrameManager() = default;

        FrameManager(const FrameManager&)             = delete;
        FrameManager(FrameManager&&)                  = delete;
        FrameManager& operator= (const FrameManager&) = delete;
        FrameManager& operator= (FrameManager&&)      = delete;

        // std::size_t is the flying frame index
        // U32 is the swapchain image's index
        [[nodiscard]] std::expected<void, Frame::ResizeNeeded> recordAndDisplay(
            std::function<void(std::size_t, vk::CommandBuffer, U32)>);
    private:
        vk::Fence                         previous_frame_finished_fence;
        std::array<Frame, FramesInFlight> flying_frames;
        std::size_t                       flying_frame_index;
    };

} // namespace gfx::vulkan