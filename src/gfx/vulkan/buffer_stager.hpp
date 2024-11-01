#pragma once

#include "buffer.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include <type_traits>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    class Allocator;

    class BufferStager
    {
    public:

        explicit BufferStager(const Allocator*);
        ~BufferStager() = default;

        BufferStager(const BufferStager&)             = delete;
        BufferStager(BufferStager&&)                  = delete;
        BufferStager& operator= (const BufferStager&) = delete;
        BufferStager& operator= (BufferStager&&)      = delete;

        template<class T>
            requires std::is_trivially_copyable_v<T>
        void enqueueTransfer(const Buffer<T>& buffer, u32 offset, std::span<T> data) const
        {
            this->enqueueByteTransfer(
                *buffer,
                offset,
                std::span<const std::byte> {// NOLINTNEXTLINE
                                            reinterpret_cast<const std::byte*>(data.data()),
                                            data.size_bytes()});
        }

        void enqueueByteTransfer(vk::Buffer, u32 offset, std::span<const std::byte>) const;

        void flushTransfers(vk::CommandBuffer) const;

    private:
        struct BufferTransfer
        {
            util::RangeAllocation staging_allocation;
            vk::Buffer            output_buffer;
            u32                   output_offset;
            u32                   size;
            u32                   frames_alive;
        };

        const Allocator*                       allocator;
        mutable gfx::vulkan::Buffer<std::byte> staging_buffer;

        util::Mutex<util::RangeAllocator>        transfer_allocator;
        util::Mutex<std::vector<BufferTransfer>> transfers;
    };
} // namespace gfx::vulkan
