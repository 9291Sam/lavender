#pragma once

#include "buffer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include <vector>

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
            std::size_t             elements);

        ~TrackedBuffer() = default;

        TrackedBuffer(const TrackedBuffer&)                 = delete;
        TrackedBuffer(TrackedBuffer&&) noexcept             = default;
        TrackedBuffer& operator= (const TrackedBuffer&)     = delete;
        TrackedBuffer& operator= (TrackedBuffer&&) noexcept = default;

        vk::Buffer operator* () const
        {
            return *this->gpu_buffer;
        }

        std::span<const T> read(std::size_t offset, std::size_t size)
        {
            return {
                this->gpu_buffer.getDataNonCoherent().data() + offset, size};
        }

        T& modify(std::size_t offset)
        {
            this->flushes.insert(
                FlushData {.offset_elements {offset}, .size_elements {1}});

            return this->gpu_buffer.getDataNonCoherent()[offset];
        }

        void write(std::size_t offset_, std::span<const T> data)
        {
            this->flushes.insert(FlushData {
                .offset_elements {offset_}, .size_elements {data.size()}});

            std::span<T> bufferData = this->gpu_buffer.getDataNonCoherent();

            std::memcpy(
                bufferData.data() + offset_, data.data(), data.size_bytes());
        }

        void flush()
        {
            this->gpu_buffer.flush(this->flushes);
            this->flushes.clear();
        }



    private:
        std::vector<FlushData> flushes;
        std::vector<T>         cpu_buffer;
        Buffer<T>              gpu_buffer;
    };

} // namespace gfx::vulkan
