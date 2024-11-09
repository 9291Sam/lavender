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
        void enqueueTransfer(const GpuOnlyBuffer<T>& buffer, u32 offset, std::span<T> data) const
        {
            this->enqueueByteTransfer(
                *buffer,
                offset, // NOLINTNEXTLINE
                std::span {reinterpret_cast<const std::byte*>(data.data()), data.size_bytes()});
        }

        void flushTransfers(vk::CommandBuffer, vk::Fence flushFinishFence) const;

    private:

        void enqueueByteTransfer(vk::Buffer, u32 offset, std::span<const std::byte>) const;

        struct BufferTransfer
        {
            util::RangeAllocation staging_allocation;
            vk::Buffer            output_buffer;
            u32                   output_offset;
            u32                   size;
        };

        const Allocator*                                allocator;
        mutable gfx::vulkan::WriteOnlyBuffer<std::byte> staging_buffer;

        util::Mutex<util::RangeAllocator>                                       transfer_allocator;
        util::Mutex<std::vector<BufferTransfer>>                                transfers;
        util::Mutex<std::unordered_map<vk::Fence, std::vector<BufferTransfer>>> transfers_to_free;
    };
} // namespace gfx::vulkan
