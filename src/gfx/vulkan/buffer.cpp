#include "buffer.hpp"
#include "allocator.hpp"
#include "device.hpp"
#include <util/log.hpp>
#include <util/threads.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::vulkan
{
    Buffer::Buffer()
        : allocator {nullptr}
        , buffer {nullptr}
        , allocation {nullptr}
        , size_bytes {~std::size_t {0}}
        , name {"EMPTY GFX::BUFFER"}
        , mapped_memory {nullptr}
    {}

    Buffer::Buffer(
        const Allocator*        allocator_,
        std::size_t             sizeBytes,
        vk::BufferUsageFlags    usage,
        vk::MemoryPropertyFlags memoryPropertyFlags,
        std::string             name_)
        : allocator {allocator_}
        , buffer {nullptr}
        , allocation {nullptr}
        , size_bytes {sizeBytes}
        , name {std::move(name_)}
        , mapped_memory {nullptr}
    {
        util::assertFatal(
            !(memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent),
            "Tried to create coherent buffer!");

        const VkBufferCreateInfo bufferCreateInfo {
            .sType {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
            .pNext {nullptr},
            .flags {},
            .size {this->size_bytes},
            .usage {static_cast<VkBufferUsageFlags>(usage)},
            .sharingMode {VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount {0},
            .pQueueFamilyIndices {nullptr},
        };

        const VmaAllocationCreateInfo allocationCreateInfo {
            .flags {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
            .usage {VMA_MEMORY_USAGE_AUTO},
            .requiredFlags {
                static_cast<VkMemoryPropertyFlags>(memoryPropertyFlags)},
            .preferredFlags {},
            .memoryTypeBits {},
            .pool {nullptr},
            .pUserData {},
            .priority {},
        };

        VkBuffer outputBuffer = nullptr;

        const vk::Result result {::vmaCreateBuffer(
            **this->allocator,
            &bufferCreateInfo,
            &allocationCreateInfo,
            &outputBuffer,
            &this->allocation,
            nullptr)};

        util::assertFatal(
            result == vk::Result::eSuccess,
            "Failed to allocate buffer {} | Size: {}",
            vk::to_string(vk::Result {result}),
            this->size_bytes);

        this->buffer = outputBuffer;
    }

    Buffer::~Buffer()
    {
        this->free();
    }

    Buffer::Buffer(Buffer&& other) noexcept
        : allocator {other.allocator}
        , buffer {other.buffer}
        , allocation {other.allocation}
        , size_bytes {other.size_bytes}
        , mapped_memory {
              other.mapped_memory.exchange(nullptr, std::memory_order_seq_cst)}
    {
        other.allocator  = nullptr;
        other.buffer     = nullptr;
        other.allocation = nullptr;
        other.size_bytes = 0;
    }

    Buffer& Buffer::operator= (Buffer&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->~Buffer();

        new (this) Buffer {std::move(other)};

        return *this;
    }

    vk::Buffer Buffer::operator* () const
    {
        return vk::Buffer {this->buffer};
    }

    std::byte* Buffer::getMappedData() const
    {
        /// This suspicious usage of atomics isn't actually a data race because
        /// of how vmaMapMemory is implemented
        if (this->mapped_memory == nullptr)
        {
            void* outputMappedMemory = nullptr;

            const VkResult result = ::vmaMapMemory(
                **this->allocator, this->allocation, &outputMappedMemory);

            util::assertFatal(
                result == VK_SUCCESS,
                "Failed to map buffer memory {}",
                vk::to_string(vk::Result {result}));

            util::assertFatal(
                outputMappedMemory != nullptr,
                "Mapped ptr was nullptr! | {}",
                vk::to_string(vk::Result {result}));

            this->mapped_memory.store(
                static_cast<std::byte*>(outputMappedMemory),
                std::memory_order_seq_cst);
        }

        return this->mapped_memory;
    }

    std::span<const std::byte> Buffer::getDataNonCoherent() const
    {
        return {this->getMappedData(), this->size_bytes};
    }

    std::span<std::byte> Buffer::getDataNonCoherent()
    {
        return {this->getMappedData(), this->size_bytes};
    }

    void Buffer::flush(std::span<const FlushData> flushes)
    {
        VkResult result = VK_ERROR_UNKNOWN;

        if (flushes.size() == 1)
        {
            result = vmaFlushAllocation(
                **this->allocator,
                this->allocation,
                flushes[0].offset,
                flushes[0].size);
        }
        else
        {
            const std::size_t numberOfFlushes = flushes.size();

            std::vector<VmaAllocation> allocations {};
            allocations.resize(numberOfFlushes, this->allocation);

            std::vector<vk::DeviceSize> offsets;
            std::vector<vk::DeviceSize> sizes;

            offsets.reserve(numberOfFlushes);
            sizes.reserve(numberOfFlushes);

            for (const FlushData& f : flushes)
            {
                offsets.push_back(f.offset);
                sizes.push_back(f.size);
            }

            result = vmaFlushAllocations(
                **this->allocator,
                static_cast<U32>(numberOfFlushes),
                allocations.data(),
                offsets.data(),
                sizes.data());
        }

        util::assertFatal(
            result == VK_SUCCESS,
            "Buffer flush failed | {}",
            vk::to_string(vk::Result {result}));
    }

    void Buffer::free()
    {
        if (this->allocator == nullptr)
        {
            return;
        }

        if (this->mapped_memory != nullptr)
        {
            ::vmaUnmapMemory(**this->allocator, this->allocation);
            this->mapped_memory = nullptr;
        }

        ::vmaDestroyBuffer(**this->allocator, this->buffer, this->allocation);
    }

} // namespace gfx::vulkan