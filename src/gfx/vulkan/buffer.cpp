#include "buffer.hpp"
#include "allocator.hpp"
#include "frame_manager.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/offsetAllocator.hpp"
#include "util/range_allocator.hpp"
#include <boost/container/small_vector.hpp>
#include <limits>
#include <source_location>
#include <type_traits>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::vulkan
{
    std::atomic<std::size_t> bufferBytesAllocated = 0; // NOLINT

    static constexpr std::size_t StagingBufferSize = std::size_t {32} * 1024 * 1024;

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
        vk::Buffer                 buffer,
        u32                        offset,
        std::span<const std::byte> dataToWrite,
        std::source_location       location) const
    {
        util::assertFatal<std::size_t>(
            dataToWrite.size_bytes() < StagingBufferSize / 2,
            "Buffer::enqueueByteTransfer of size {} is too large",
            dataToWrite.size_bytes(),
            location);

        util::assertWarn<std::size_t>(
            dataToWrite.size_bytes() > 0,
            "BufferStager::enqueueByteTransfer of size {} is too small",
            dataToWrite.size_bytes(),
            location);

        std::expected<util::RangeAllocation, util::RangeAllocator::OutOfBlocks> maybeAllocation =
            this->transfer_allocator.lock(
                [&](util::RangeAllocator& a)
                {
                    return a.tryAllocate(static_cast<u32>(dataToWrite.size_bytes()));
                });

        if (maybeAllocation.has_value())
        {
            this->allocated += dataToWrite.size_bytes();

            std::span<std::byte> stagingBufferData = this->staging_buffer.getGpuDataNonCoherent();

            // static_assert(std::is_trivially_copyable_v<std::byte>);
            std::memcpy(
                stagingBufferData.data() + maybeAllocation->offset,
                dataToWrite.data(),
                dataToWrite.size_bytes());

            this->transfers.lock(
                [&](std::vector<BufferTransfer>& t)
                {
                    t.push_back(BufferTransfer {
                        .staging_allocation {*maybeAllocation},
                        .output_buffer {buffer},
                        .output_offset {offset},
                        .size {static_cast<u32>(dataToWrite.size_bytes())},
                    });
                });
        }
        else
        {
            // shit we need to overflow

            this->overflow_transfers.lock(
                [&](std::vector<OverflowTransfer>& overflowTransfers)
                {
                    overflowTransfers.push_back({OverflowTransfer {
                        .buffer {buffer},
                        .offset {offset},
                        .data {dataToWrite.begin(), dataToWrite.end()},
                        .location {location}}});
                });
        }
    }

    void BufferStager::flushTransfers(
        vk::CommandBuffer commandBuffer, std::shared_ptr<vk::UniqueFence> flushFinishFence) const
    {
        // Free all allocations that have already completed.
        this->transfers_to_free.lock(
            [&](std::unordered_map<std::shared_ptr<vk::UniqueFence>, std::vector<BufferTransfer>>&
                    toFreeMap)
            {
                std::vector<std::shared_ptr<vk::UniqueFence>> toRemove {};

                for (const auto& [fence, allocations] : toFreeMap)
                {
                    if (this->allocator->getDevice()->getDevice().getFenceStatus(**fence)
                        == vk::Result::eSuccess)
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

                for (const std::shared_ptr<vk::UniqueFence>& f : toRemove)
                {
                    toFreeMap.erase(f);
                }
            });

        std::vector<BufferTransfer> grabbedTransfers = this->transfers.moveInner();

        std::unordered_map<vk::Buffer, std::vector<vk::BufferCopy>> copies {};
        std::vector<FlushData>                                      stagingFlushes {};

        for (const BufferTransfer& transfer : grabbedTransfers)
        {
            if (transfer.size == 0)
            {
                util::logWarn("zst transfer!");

                continue;
            }
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
            if (bufferCopies.size() > 4096)
            {
                util::logWarn("Excessive copies on buffer {}", bufferCopies.size());
            }
            commandBuffer.copyBuffer(*this->staging_buffer, outputBuffer, bufferCopies);
        }

        this->staging_buffer.flush(stagingFlushes);

        this->transfers_to_free.lock(
            [&](std::unordered_map<std::shared_ptr<vk::UniqueFence>, std::vector<BufferTransfer>>&
                    toFreeMap)
            {
                toFreeMap[flushFinishFence].insert(
                    toFreeMap[flushFinishFence].begin(),
                    std::make_move_iterator(grabbedTransfers.begin()),
                    std::make_move_iterator(grabbedTransfers.end()));
            });

        {
            std::vector<OverflowTransfer> overflows = this->overflow_transfers.moveInner();

            if (!overflows.empty())
            {
                util::logWarn("{} buffer transfer overflows this frame!", overflows.size());
            }

            for (OverflowTransfer& t : overflows)
            {
                this->enqueueByteTransfer(t.buffer, t.offset, t.data, t.location);
            }
        }
    }

    std::pair<std::size_t, std::size_t> BufferStager::getUsage() const
    {
        return {this->allocated.load(), StagingBufferSize};
    }

} // namespace gfx::vulkan
