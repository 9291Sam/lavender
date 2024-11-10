#include "buffer.hpp"
#include "allocator.hpp"
#include "frame_manager.hpp"
#include "util/offsetAllocator.hpp"
#include "util/range_allocator.hpp"
#include <boost/container/small_vector.hpp>
#include <limits>
#include <type_traits>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::vulkan
{
    static constexpr std::size_t StagingBufferSize = 192UZ * 1024 * 1024;

    BufferStager::BufferStager(const Allocator* allocator_)
        : allocator {allocator_}
        , staging_buffer {
              this->allocator,
              vk::BufferUsageFlagBits::eTransferSrc,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,  StagingBufferSize,
              "Staging Buffer"
          }
        , transfer_allocator {util::RangeAllocator {StagingBufferSize, 1024 * 128}}
        , transfers {std::vector<BufferTransfer> {}}
    {}

    void BufferStager::enqueueByteTransfer(
        vk::Buffer buffer, u32 offset, std::span<const std::byte> dataToWrite) const
    {
        util::assertFatal(
            dataToWrite.size_bytes() < std::numeric_limits<u32>::max(),
            "Buffer::enqueueByteTransfer of size {} is too large",
            dataToWrite.size_bytes());

        util::RangeAllocation thisAllocation = this->transfer_allocator.lock(
            [&](util::RangeAllocator& a)
            {
                this->allocated += dataToWrite.size_bytes();

                return a.allocate(static_cast<u32>(dataToWrite.size_bytes()));
            });

        std::span<std::byte> stagingBufferData = this->staging_buffer.getDataNonCoherent();

        // static_assert(std::is_trivially_copyable_v<std::byte>);
        std::memcpy(
            stagingBufferData.data() + thisAllocation.offset,
            dataToWrite.data(),
            dataToWrite.size_bytes());

        this->transfers.lock(
            [&](std::vector<BufferTransfer>& t)
            {
                t.push_back(BufferTransfer {
                    .staging_allocation {thisAllocation},
                    .output_buffer {buffer},
                    .output_offset {offset},
                    .size {static_cast<u32>(dataToWrite.size_bytes())},
                });
            });
    }

    void
    BufferStager::flushTransfers(vk::CommandBuffer commandBuffer, vk::Fence flushFinishFence) const
    {
        // Free all allocations that have already completed.
        this->transfers_to_free.lock(
            [&](std::unordered_map<vk::Fence, std::vector<BufferTransfer>>& toFreeMap)
            {
                std::vector<vk::Fence> toRemove {};

                for (const auto& [fence, allocations] : toFreeMap)
                {
                    if (this->allocator->getDevice().getFenceStatus(fence) == vk::Result::eSuccess)
                    {
                        toRemove.push_back(fence);

                        this->transfer_allocator.lock(
                            [&](util::RangeAllocator& stagingAllocator)
                            {
                                for (BufferTransfer transfer : allocations)
                                {
                                    this->allocated -= transfer.size;
                                    stagingAllocator.free(transfer.staging_allocation);
                                }
                            });
                    }
                }

                for (vk::Fence f : toRemove)
                {
                    toFreeMap.erase(f);
                }
            });

        std::vector<BufferTransfer> grabbedTransfers = this->transfers.moveInner();

        std::unordered_map<vk::Buffer, std::vector<vk::BufferCopy>> copies {};
        std::vector<FlushData>                                      stagingFlushes {};

        for (const BufferTransfer& transfer : grabbedTransfers)
        {
            copies[transfer.output_buffer].push_back(vk::BufferCopy {
                .srcOffset {transfer.staging_allocation.offset},
                .dstOffset {transfer.output_offset},
                .size {transfer.size},
            });

            stagingFlushes.push_back(FlushData {
                .offset_elements {transfer.staging_allocation.offset},
                .size_elements {transfer.size}});
        }

        for (const auto& [outputBuffer, bufferCopies] : copies)
        {
            if (bufferCopies.size() > 16)
            {
                util::logTrace("Excessive copies on buffer {}", bufferCopies.size());
            }
            commandBuffer.copyBuffer(*this->staging_buffer, outputBuffer, bufferCopies);
        }

        this->staging_buffer.flush(stagingFlushes);

        this->transfers_to_free.lock(
            [&](std::unordered_map<vk::Fence, std::vector<BufferTransfer>>& toFreeMap)
            {
                toFreeMap[flushFinishFence].append_range(std::move(grabbedTransfers));
            });
    }

    std::pair<std::size_t, std::size_t> BufferStager::getUsage() const
    {
        return {this->allocated.load(), StagingBufferSize};
    }

} // namespace gfx::vulkan