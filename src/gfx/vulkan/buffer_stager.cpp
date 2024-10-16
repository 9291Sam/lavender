#include "buffer_stager.hpp"
#include "allocator.hpp"
#include "buffer.hpp"
#include "frame_manager.hpp"
#include "util/offsetAllocator.hpp"
#include "util/range_allocator.hpp"
#include <boost/container/small_vector.hpp>
#include <limits>
#include <type_traits>
#include <vulkan/vulkan_enums.hpp>


namespace gfx::vulkan
{
    static constexpr std::size_t StagingBufferSize = 128UZ * 1024 * 1024;

    BufferStager::BufferStager(const Allocator* allocator_)
        : allocator {allocator_}
        , staging_buffer {
              this->allocator,
              vk::BufferUsageFlagBits::eTransferSrc,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,  StagingBufferSize,
          }
        , transfer_allocator {util::RangeAllocator {StagingBufferSize, 1024 * 64}}
        , transfers {std::vector<BufferTransfer> {}}
    {}

    void BufferStager::enqueueTransfer(
        vk::Buffer                 buffer,
        u32                        offset,
        std::span<const std::byte> dataToWrite) const
    {
        util::assertFatal(
            dataToWrite.size_bytes() < std::numeric_limits<u32>::max(),
            "Buffer::enqueueTransfer of size {} is too large",
            dataToWrite.size_bytes());

        util::RangeAllocation thisAllocation = this->transfer_allocator.lock(
            [&](util::RangeAllocator& a)
            {
                return a.allocate(static_cast<u32>(dataToWrite.size_bytes()));
            });

        std::span<std::byte> stagingBufferData =
            this->staging_buffer.getDataNonCoherent();

        static_assert(std::is_trivially_copyable_v<std::byte>);
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
                    .frames_alive {0}});
            });
    }

    void BufferStager::flushTransfers(vk::CommandBuffer commandBuffer) const
    {
        std::vector<util::RangeAllocation> allocationsToFree {};
        std::vector<FlushData>             stagingFlushes {};

        this->transfers.lock(
            [&](std::vector<BufferTransfer>& lockedTransfers)
            {
                std::vector<BufferTransfer> transfersToRetain {};
                transfersToRetain.reserve(lockedTransfers.size());

                for (BufferTransfer t : lockedTransfers)
                {
                    if (t.frames_alive == 0)
                    {
                        stagingFlushes.push_back(FlushData {
                            .offset_elements {t.staging_allocation.offset},
                            .size_elements {t.size}});

                        const vk::BufferCopy bufferCopy {
                            .srcOffset {t.staging_allocation.offset},
                            .dstOffset {t.output_offset},
                            .size {t.size},
                        };

                        commandBuffer.copyBuffer(
                            *this->staging_buffer,
                            t.output_buffer,
                            1,
                            &bufferCopy);
                    }

                    if (t.frames_alive > FramesInFlight)
                    {
                        allocationsToFree.push_back(t.staging_allocation);
                    }
                    else
                    {
                        transfersToRetain.push_back(t);
                    }

                    t.frames_alive += 1;
                }

                lockedTransfers = transfersToRetain;
            });

        this->staging_buffer.flush(stagingFlushes);

        this->transfer_allocator.lock(
            [&](util::RangeAllocator& allocator_)
            {
                for (util::RangeAllocation allocation : allocationsToFree)
                {
                    allocator_.free(allocation);
                }
            });
    }

} // namespace gfx::vulkan
