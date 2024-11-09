#pragma once

#include "gfx/vulkan/allocator.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <type_traits>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::vulkan
{
    class Allocator;
    class Device;

    struct FlushData
    {
        vk::DeviceSize offset_elements;
        vk::DeviceSize size_elements;
    };

    static inline std::atomic<std::size_t> bufferBytesAllocated; // NOLINT

    template<class T>
        requires std::is_trivially_copyable_v<T>
    class GpuOnlyBuffer
    {
    public:

        GpuOnlyBuffer()
            : allocator {nullptr}
            , buffer {nullptr}
            , allocation {nullptr}
            , elements {0}
        {}
        GpuOnlyBuffer(
            const Allocator*        allocator_,
            vk::BufferUsageFlags    usage,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements_,
            std::string             name_)
            : name {std::move(name_)}
            , allocator {allocator_}
            , buffer {nullptr}
            , allocation {nullptr}
            , elements {elements_}
        {
            util::assertFatal(
                !(memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent),
                "Tried to create coherent buffer!");

            const VkBufferCreateInfo bufferCreateInfo {
                .sType {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
                .pNext {nullptr},
                .flags {},
                .size {this->elements * sizeof(T)},
                .usage {static_cast<VkBufferUsageFlags>(usage)},
                .sharingMode {VK_SHARING_MODE_EXCLUSIVE},
                .queueFamilyIndexCount {0},
                .pQueueFamilyIndices {nullptr},
            };

            const VmaAllocationCreateInfo allocationCreateInfo {
                .flags {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
                .usage {VMA_MEMORY_USAGE_AUTO},
                .requiredFlags {static_cast<VkMemoryPropertyFlags>(memoryPropertyFlags)},
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
                this->elements * sizeof(T));

            this->buffer = outputBuffer;

            if constexpr (util::isDebugBuild())
            {
                this->allocator->getDevice().setDebugUtilsObjectNameEXT(
                    vk::DebugUtilsObjectNameInfoEXT {
                        .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                        .pNext {nullptr},
                        .objectType {vk::ObjectType::eBuffer},
                        .objectHandle {std::bit_cast<u64>(this->buffer)},
                        .pObjectName {this->name.c_str()},
                    });
            }

            bufferBytesAllocated += (this->elements * sizeof(T));
        }
        ~GpuOnlyBuffer()
        {
            this->free();
        }

        GpuOnlyBuffer(const GpuOnlyBuffer&) = delete;
        GpuOnlyBuffer& operator= (GpuOnlyBuffer&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~GpuOnlyBuffer();

            new (this) GpuOnlyBuffer {std::move(other)};

            return *this;
        }
        GpuOnlyBuffer& operator= (const GpuOnlyBuffer&) = delete;
        GpuOnlyBuffer(GpuOnlyBuffer&& other) noexcept
            : allocator {other.allocator}
            , buffer {other.buffer}
            , allocation {other.allocation}
            , elements {other.elements}
        {
            other.allocator  = nullptr;
            other.buffer     = nullptr;
            other.allocation = nullptr;
            other.elements   = 0;
        }

        vk::Buffer operator* () const
        {
            return vk::Buffer {this->buffer};
        }

    protected:

        void free()
        {
            if (this->allocator == nullptr)
            {
                return;
            }

            ::vmaDestroyBuffer(**this->allocator, this->buffer, this->allocation);

            bufferBytesAllocated -= (this->elements * sizeof(T));
        }

        std::string      name;
        const Allocator* allocator;
        vk::Buffer       buffer;
        VmaAllocation    allocation;
        std::size_t      elements;
    };

    template<class T>
    class WriteOnlyBuffer : public GpuOnlyBuffer<T>
    {
    public:

        WriteOnlyBuffer(
            const Allocator*        allocator_,
            vk::BufferUsageFlags    usage,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements_,
            std::string             name_)
            : gfx::vulkan::GpuOnlyBuffer<T> {
                  allocator_, usage, memoryPropertyFlags, elements_, std::move(name_)}
        {}
        ~WriteOnlyBuffer()
        {
            this->free();
        }

        WriteOnlyBuffer(const WriteOnlyBuffer&) = delete;
        WriteOnlyBuffer& operator= (WriteOnlyBuffer&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~Buffer();

            new (this) WriteOnlyBuffer {std::move(other)};

            return *this;
        }
        WriteOnlyBuffer& operator= (const WriteOnlyBuffer&) = delete;
        WriteOnlyBuffer(WriteOnlyBuffer&& other) noexcept
            : GpuOnlyBuffer<T> {std::move(other)}
            , mapped_memory {other.mapped_memory.load()}
        {
            other.mapped_memory = nullptr;
        }

        std::span<const T> getDataNonCoherent() const
        {
            return {this->getMappedData(), this->elements};
        }

        std::span<T> getDataNonCoherent()
        {
            return {this->getMappedData(), this->elements};
        }

        void flush(std::span<const FlushData> flushes)
        {
            VkResult result = VK_ERROR_UNKNOWN;

            if (flushes.size() == 1)
            {
                result = vmaFlushAllocation(
                    **this->allocator,
                    this->allocation,
                    flushes[0].offset_elements * sizeof(T),
                    flushes[0].size_elements * sizeof(T));
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
                    offsets.push_back(f.offset_elements * sizeof(T));
                    sizes.push_back(f.size_elements * sizeof(T));
                }

                result = vmaFlushAllocations(
                    **this->allocator,
                    static_cast<u32>(numberOfFlushes),
                    allocations.data(),
                    offsets.data(),
                    sizes.data());
            }

            util::assertFatal(
                result == VK_SUCCESS,
                "Buffer flush failed | {}",
                vk::to_string(vk::Result {result}));
        }

    private:

        void free()
        {
            if (this->mapped_memory != nullptr)
            {
                ::vmaUnmapMemory(**this->allocator, this->allocation);
                this->mapped_memory = nullptr;
            }
        }

        T* getMappedData() const
        {
            /// This suspicious usage of atomics isn't actually a data race
            /// because of how vmaMapMemory is implemented
            if (this->mapped_memory == nullptr)
            {
                void* outputMappedMemory = nullptr;

                const VkResult result =
                    ::vmaMapMemory(**this->allocator, this->allocation, &outputMappedMemory);

                util::assertFatal(
                    result == VK_SUCCESS,
                    "Failed to map buffer memory {}",
                    vk::to_string(vk::Result {result}));

                util::assertFatal(
                    outputMappedMemory != nullptr,
                    "Mapped ptr was nullptr! | {}",
                    vk::to_string(vk::Result {result}));

                this->mapped_memory.store(
                    static_cast<T*>(outputMappedMemory), std::memory_order_seq_cst);
            }

            return this->mapped_memory;
        }

        mutable std::atomic<T*> mapped_memory;
    };

    template<class T>
    class CpuCachedBuffer : private WriteOnlyBuffer<T>
    {
    public:

        void write(std::size_t offset, std::span<const T>);
        void write(std::size_t, const T&);

        std::span<const T> read(std::size_t offset, std::size_t size);
        const T&           read(std::size_t) const;

        std::span<T> modify(std::size_t offset, std::size_t size);
        T&           modify(std::size_t);

        // void flushViaStager(const gfx::vulkan::BufferStager& stager)
        // {
        //     this->mergeFlushes();

        //     for (const FlushData& f : this->flushes)
        //     {
        //         stager.enqueueTransfer(
        //             this->gpu_buffer,
        //             f.offset_elements,
        //             {this->cpu_buffer.data() + f.offset_elements,
        //              this->cpu_buffer.data() + f.offset_elements + f.size_elements});
        //     }

        //     this->flushes.clear();
        // }

    private:
        std::vector<T>         cpu_buffer;
        std::vector<FlushData> flushes;
    };
} // namespace gfx::vulkan
