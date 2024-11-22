#pragma once

#include "gfx/vulkan/allocator.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include "util/ranges.hpp"
#include <ctti/nameof.hpp>
#include <type_traits>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    class Allocator;
    class Device;
    class BufferStager;

    struct FlushData
    {
        vk::DeviceSize offset_elements;
        vk::DeviceSize size_elements;
    };

    extern std::atomic<std::size_t> bufferBytesAllocated; // NOLINT

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

        void uploadImmediate(u32 offset, std::span<const T> payload)
            requires std::is_copy_constructible_v<T>
        {
            std::copy(
                payload.begin(), payload.end(), this->getGpuDataNonCoherent().data() + offset);

            const gfx::vulkan::FlushData flush {
                .offset_elements {offset},
                .size_elements {payload.size()},
            };

            this->flush({&flush, 1});
        }

        void fillImmediate(const T& value)
        {
            const std::span<T> data = this->getGpuDataNonCoherent();

            std::fill(data.begin(), data.end(), value);

            const gfx::vulkan::FlushData flush {
                .offset_elements {0},
                .size_elements {this->elements},
            };

            this->flush({&flush, 1});
        }

        std::size_t getSizeBytes()
        {
            return this->elements * sizeof(T);
        }

        friend class BufferStager;

        std::span<const T> getGpuDataNonCoherent() const
        {
            return {this->getMappedData(), this->elements};
        }

        std::span<T> getGpuDataNonCoherent()
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
    class CpuCachedBuffer : public WriteOnlyBuffer<T>
    {
    public:

        CpuCachedBuffer(
            const Allocator*        allocator_,
            vk::BufferUsageFlags    usage,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements_,
            std::string             name_)
            : gfx::vulkan::WriteOnlyBuffer<T> {
                  allocator_, usage, memoryPropertyFlags, elements_, std::move(name_)}
        {
            util::assertWarn(
                static_cast<bool>(vk::BufferUsageFlagBits::eTransferDst | usage),
                "Creating CpuCachedBuffer<{}> without vk::BufferUsageFlagBits::eTransferDst",
                ctti::ctti_nameof<T>({}).cppstring());

            this->cpu_buffer.resize(this->elements);
        }
        ~CpuCachedBuffer() = default;

        CpuCachedBuffer(const CpuCachedBuffer&) = delete;
        CpuCachedBuffer& operator= (CpuCachedBuffer&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~Buffer();

            new (this) CpuCachedBuffer {std::move(other)};

            return *this;
        }
        CpuCachedBuffer& operator= (const CpuCachedBuffer&) = delete;
        CpuCachedBuffer(CpuCachedBuffer&& other) noexcept
            : WriteOnlyBuffer<T> {std::move(other)}
            , cpu_buffer {std::move(other.cpu_buffer)}
            , flushes {std::move(other.flushes)}
        {}

        std::span<const T> read(std::size_t offset, std::size_t size) const
        {
            return std::span<const T> {&this->cpu_buffer[offset], size};
        }
        const T& read(std::size_t offset) const
        {
            return this->cpu_buffer[offset];
        }

        void write(std::size_t offset, std::span<const T> data)
        {
            this->flushes.push_back(
                util::InclusiveRange {.start {offset}, .end {offset + data.size() - 1}});

            std::memcpy(&this->cpu_buffer[offset], data.data(), data.size_bytes());
        }
        void write(std::size_t offset, const T& t)
        {
            this->write(offset, std::span<const T> {&t, 1});
        }

        std::span<T> modify(std::size_t offset, std::size_t size)
        {
            util::assertFatal(size > 0, "dont do this");

            this->flushes.push_back(
                util::InclusiveRange {.start {offset}, .end {offset + size - 1}});

            return std::span<T> {&this->cpu_buffer[offset], size};
        }
        T& modify(std::size_t offset)
        {
            util::assertFatal(!this->cpu_buffer.empty(), "hmmm");

            return this->modify(offset, 1)[0];
        }

        void flushViaStager(const BufferStager& stager);

        std::vector<FlushData> mergeFlushes()
        {
            std::vector<util::InclusiveRange> mergedRanges;

            mergedRanges = util::mergeDownRanges(std::move(this->flushes), 128);

            std::vector<FlushData> newFlushes {};
            newFlushes.reserve(mergedRanges.size());

            for (const auto& r : mergedRanges)
            {
                newFlushes.push_back(
                    FlushData {.offset_elements {r.start}, .size_elements {r.size()}});
            }

            return newFlushes;
        }
    private:
        std::vector<T>                    cpu_buffer;
        std::vector<util::InclusiveRange> flushes;
    };

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
        void
        enqueueTransfer(const GpuOnlyBuffer<T>& buffer, u32 offset, std::span<const T> data) const
        {
            this->enqueueByteTransfer(
                *buffer,
                offset * sizeof(T), // NOLINTNEXTLINE
                std::span {reinterpret_cast<const std::byte*>(data.data()), data.size_bytes()});
        }

        void flushTransfers(vk::CommandBuffer, vk::Fence flushFinishFence) const;

        std::pair<std::size_t, std::size_t> getUsage() const;

    private:

        void enqueueByteTransfer(vk::Buffer, u32 offset, std::span<const std::byte>) const;

        struct BufferTransfer
        {
            util::RangeAllocation staging_allocation;
            vk::Buffer            output_buffer;
            u32                   output_offset;
            u32                   size;
        };

        mutable std::atomic<std::size_t>                allocated;
        const Allocator*                                allocator;
        mutable gfx::vulkan::WriteOnlyBuffer<std::byte> staging_buffer;

        util::Mutex<util::RangeAllocator>                                       transfer_allocator;
        util::Mutex<std::vector<BufferTransfer>>                                transfers;
        util::Mutex<std::unordered_map<vk::Fence, std::vector<BufferTransfer>>> transfers_to_free;
    };

    template<class T>
    void CpuCachedBuffer<T>::flushViaStager(const BufferStager& stager)
    {
        std::vector<FlushData> mergedFlushes = this->mergeFlushes();

        for (const FlushData& f : mergedFlushes)
        {
            stager.enqueueTransfer(
                *this,
                static_cast<u32>(f.offset_elements),
                {this->cpu_buffer.data() + f.offset_elements, f.size_elements});
        }
    }

} // namespace gfx::vulkan
