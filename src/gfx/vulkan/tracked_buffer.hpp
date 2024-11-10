#pragma once

#include "buffer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/buffer_stager.hpp"
#include "util/misc.hpp"
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::vulkan
{
    template<class T>
    class TrackedBuffer
    {
    public:
        TrackedBuffer() = default;
        TrackedBuffer(
            const Allocator*        allocator,
            vk::BufferUsageFlags    usage,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements,
            std::string             name)
            : gpu_buffer {allocator, usage, memoryPropertyFlags, elements, std::move(name)}
        {
            this->cpu_buffer.resize(elements);
        }

        ~TrackedBuffer() = default;

        TrackedBuffer(const TrackedBuffer&)                 = delete;
        TrackedBuffer(TrackedBuffer&&) noexcept             = default;
        TrackedBuffer& operator= (const TrackedBuffer&)     = delete;
        TrackedBuffer& operator= (TrackedBuffer&&) noexcept = default;

        vk::Buffer operator* () const
        {
            return *this->gpu_buffer;
        }

        const T& read(std::size_t offset)
        {
            return this->cpu_buffer.data()[offset];
        }

        std::span<const T> read(std::size_t offset, std::size_t size)
        {
            return {this->cpu_buffer.data() + offset, size};
        }

        T& modify(std::size_t offset)
        {
            this->flushes.push_back(FlushData {.offset_elements {offset}, .size_elements {1}});

            return this->cpu_buffer[offset];
        }

        void write(std::size_t offset, const T& t)
        {
            this->write(offset, std::span {&t, 1});
        }

        void write(std::size_t offset_, std::span<const T> data)
        {
            this->flushes.push_back(
                FlushData {.offset_elements {offset_}, .size_elements {data.size()}});

            std::memcpy(this->cpu_buffer.data() + offset_, data.data(), data.size_bytes());
        }

        void flushViaStager(const gfx::vulkan::BufferStager& stager)
        {
            this->mergeFlushes();

            for (const FlushData& f : this->flushes)
            {
                stager.enqueueTransfer(
                    this->gpu_buffer,
                    static_cast<u32>(f.offset_elements),
                    {this->cpu_buffer.data() + f.offset_elements, f.size_elements});
            }

            this->flushes.clear();

            // this->flush()
        }

        void flush()
        {
            T* gpuData = this->gpu_buffer.getDataNonCoherent().data();

            this->mergeFlushes();

            for (const FlushData& f : this->flushes)
            {
                std::memcpy(
                    gpuData + f.offset_elements,
                    this->cpu_buffer.data() + f.offset_elements,
                    f.size_elements * sizeof(T));
            }

            this->gpu_buffer.flush(this->flushes);
            this->flushes.clear();
        }

        // void flushWhole(bool force = false)
        // {
        //     if (!this->flushes.empty() || force)
        //     {
        //         std::span<T>       gpuData = this->gpu_buffer.getDataNonCoherent();
        //         std::span<const T> cpuData = this->cpu_buffer;

        //         std::memcpy(gpuData.data(), cpuData.data(), cpuData.size_bytes());

        //         const gfx::vulkan::FlushData wholeFlush {
        //             .offset_elements {0}, .size_elements {this->cpu_buffer.size()}};

        //         this->gpu_buffer.flush({&wholeFlush, 1});
        //         this->flushes.clear();
        //     }
        // }

        void mergeFlushes()
        {
            // util::Timer t {"merge"};

            std::vector<std::size_t> points {};

            for (const FlushData& f : this->flushes)
            {
                for (std::size_t i = 0; i < f.size_elements; ++i)
                {
                    points.push_back(i + f.offset_elements);
                }
            }

            std::vector<std::pair<std::size_t, std::size_t>> merged =
                util::combineIntoRanges(points, 65535, 4);

            std::vector<FlushData> newFlushes {};
            newFlushes.reserve(merged.size());

            for (const auto& [from, to] : merged)
            {
                newFlushes.push_back(
                    FlushData {.offset_elements {from}, .size_elements {to - from + 1}});
            }

            this->flushes = std::move(newFlushes);
        }




    private:
        std::vector<FlushData> flushes;
        std::vector<T>         cpu_buffer;
        WriteOnlyBuffer<T>     gpu_buffer;
    };

} // namespace gfx::vulkan
