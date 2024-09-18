#pragma once

#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VmaAllocator)

namespace gfx::vulkan
{
    class Allocator;
    class Device;

    class Buffer
    {
    public:
        struct FlushData
        {
            vk::DeviceSize offset;
            vk::DeviceSize size;
        };
    public:

        Buffer();
        Buffer(
            const Allocator*,
            std::size_t sizeBytes,
            vk::BufferUsageFlags,
            vk::MemoryPropertyFlags,
            std::string name);
        ~Buffer();

        Buffer(const Buffer&) = delete;
        Buffer(Buffer&&) noexcept;
        Buffer& operator= (const Buffer&) = delete;
        Buffer& operator= (Buffer&&) noexcept;

        [[nodiscard]] vk::Buffer operator* () const;

        std::span<const std::byte> getDataNonCoherent() const;
        std::span<std::byte>       getDataNonCoherent();

        void flush(std::span<const FlushData>);




    private:
        void       free();
        std::byte* getMappedData() const;

        const Allocator* allocator;

        vk::Buffer    buffer;
        VmaAllocation allocation;

        std::size_t                     size_bytes;
        std::string                     name;
        mutable std::atomic<std::byte*> mapped_memory;
    };
} // namespace gfx::vulkan
