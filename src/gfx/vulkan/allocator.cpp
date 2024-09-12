#include "allocator.hpp"
#include "device.hpp"
#include "instance.hpp"
#include <util/log.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::vulkan
{
    Allocator::Allocator(const Instance& instance, const Device& device)
        : allocator {nullptr}
    {
        VmaVulkanFunctions vulkanFunctions {};
        vulkanFunctions.vkGetInstanceProcAddr =
            VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr =
            VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

        const VmaAllocatorCreateInfo allocatorCreateInfo {
            .flags {},
            .physicalDevice {device.getPhysicalDevice()},
            .device {device.getDevice()},
            .preferredLargeHeapBlockSize {0}, // chosen by VMA
            .pAllocationCallbacks {nullptr},
            .pDeviceMemoryCallbacks {nullptr},
            .pHeapSizeLimit {nullptr},
            .pVulkanFunctions {&vulkanFunctions},
            .instance {*instance},
            .vulkanApiVersion {instance.getVulkanVersion()},
            .pTypeExternalMemoryHandleTypes {nullptr},
        };

        const vk::Result result {
            ::vmaCreateAllocator(&allocatorCreateInfo, &this->allocator)};

        util::assertFatal(
            result == vk::Result::eSuccess,
            "Failed to create allocator | {}",
            vk::to_string(result));

        const std::array availableDescriptors {
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eSampler}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eCombinedImageSampler},
                .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eSampledImage},
                .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eStorageImage},
                .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eUniformBuffer},
                .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eStorageBuffer},
                .descriptorCount {1024}}};

        const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {
            .sType {vk::StructureType::eDescriptorPoolCreateInfo},
            .pNext {nullptr},
            .flags {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet},
            .maxSets {1024},
            .poolSizeCount {static_cast<U32>(availableDescriptors.size())},
            .pPoolSizes {availableDescriptors.data()},
        };

        this->descriptor_pool =
            device->createDescriptorPoolUnique(descriptorPoolCreateInfo);
    }

    Allocator::~Allocator()
    {
        ::vmaDestroyAllocator(this->allocator);
    }

    VmaAllocator Allocator::operator* () const
    {
        return this->allocator;
    }

} // namespace gfx::vulkan