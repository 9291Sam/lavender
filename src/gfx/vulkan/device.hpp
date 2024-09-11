#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include <thread>
#include <util/threads.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    class Device
    {
    public:
        enum class QueueType : U8
        {
            Graphics           = 0,
            AsyncCompute       = 1,
            AsyncTransfer      = 2,
            NumberOfQueueTypes = 3,
        };

        static const char* getStringOfQueueType(QueueType t)
        {
            switch (t)
            {
            case QueueType::Graphics:
                return "Graphics";
            case QueueType::AsyncCompute:
                return "AsyncCompute";
            case QueueType::AsyncTransfer:
                return "AsyncTransfer";
            case QueueType::NumberOfQueueTypes:
                [[fallthrough]];
            default:
                return "";
            }
        }
    public:
        Device(vk::Instance, vk::SurfaceKHR);
        ~Device() = default;

        Device(const Device&)             = delete;
        Device(Device&&)                  = delete;
        Device& operator= (const Device&) = delete;
        Device& operator= (Device&&)      = delete;

        [[nodiscard]] std::optional<U32>
                          getFamilyOfQueueType(QueueType) const noexcept;
        [[nodiscard]] U32 getNumberOfQueues(QueueType) const noexcept;
        [[nodiscard]] vk::PhysicalDevice getPhysicalDevice() const noexcept;
        [[nodiscard]] vk::Device         getDevice() const noexcept;

        void acquireQueue(
            QueueType                      queueType,
            std::invocable<vk::Queue> auto accessFunc) const noexcept
        {
            const std::size_t idx =
                static_cast<std::size_t>(util::toUnderlying(queueType));

            util::assertFatal(
                idx < static_cast<std::size_t>(QueueType::NumberOfQueueTypes),
                "Tried to lookup an invalid queue of type {}",
                idx);

            const std::vector<util::Mutex<vk::Queue>>& qs =
                this->queues.at(util::toUnderlying(queueType));

            while (true)
            {
                for (const util::Mutex<vk::Queue>& q : qs)
                {
                    const bool wasLockedAndExecuted = q.tryLock(
                        [&](vk::Queue lockedQueue)
                        {
                            accessFunc(lockedQueue);
                        });

                    if (wasLockedAndExecuted)
                    {
                        return;
                    }
                }

                util::logWarn(
                    "Failed to acquire a queue of type {}, retrying",
                    getStringOfQueueType(queueType));

                std::this_thread::yield();
            }
        }

    private:
        std::array<
            std::vector<util::Mutex<vk::Queue>>,
            static_cast<std::size_t>(QueueType::NumberOfQueueTypes)>
            queues;

        std::array<
            std::optional<U32>,
            static_cast<std::size_t>(QueueType::NumberOfQueueTypes)>
            queue_family_indexes;

        std::array<U32, static_cast<std::size_t>(QueueType::NumberOfQueueTypes)>
            queue_family_numbers;

        vk::PhysicalDevice physical_device;
        vk::UniqueDevice   device;
    };
} // namespace gfx::vulkan
