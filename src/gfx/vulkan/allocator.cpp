#include "allocator.hpp"
#include "device.hpp"
#include "instance.hpp"
#include <util/log.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    Allocator::Allocator(const Instance& instance, const Device& device)
        : allocator {nullptr}
        , device {device.getDevice()}
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

    vk::DescriptorSet
    Allocator::allocateDescriptorSet(vk::DescriptorSetLayout layout) const
    {
        const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {

            .sType {vk::StructureType::eDescriptorSetAllocateInfo},
            .pNext {nullptr},
            .descriptorPool {*this->descriptor_pool},
            .descriptorSetCount {1},
            .pSetLayouts {&layout},
        };
        return this->device.allocateDescriptorSets(descriptorSetAllocateInfo)
            .at(0);
    }

    void Allocator::earlyDeallocateDescriptorSet(vk::DescriptorSet set) const
    {
        this->device.freeDescriptorSets(*this->descriptor_pool, {set});
    }

    std::shared_ptr<vk::UniqueDescriptorSetLayout>
    Allocator::cacheDescriptorSetLayout(
        CacheableDescriptorSetLayoutCreateInfo info) const
    {
        return this->descriptor_set_layout_cache.lock(
            [&](auto& cache)
            {
                if (cache.contains(info))
                {
                    return cache.at(info);
                }
                else
                {
                    const vk::DescriptorSetLayoutCreateInfo
                        descriptorSetLayoutCreateInfo {

                            .sType {vk::StructureType::
                                        eDescriptorSetLayoutCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .bindingCount {
                                static_cast<U32>(info.bindings.size())},
                            .pBindings {info.bindings.data()},

                        };

                    vk::UniqueDescriptorSetLayout layout =
                        this->device.createDescriptorSetLayoutUnique(
                            descriptorSetLayoutCreateInfo);

                    std::shared_ptr<vk::UniqueDescriptorSetLayout> // NOLINT
                        sharedLayout {new vk::UniqueDescriptorSetLayout {
                            std::move(layout)}};

                    cache[info] = sharedLayout;

                    return sharedLayout;
                }
            });
    }

    std::shared_ptr<vk::UniquePipelineLayout>
    Allocator::cachePipelineLayout(CacheablePipelineLayoutCreateInfo info) const
    {
        return this->pipeline_layout_cache.lock(
            [&](auto& cache)
            {
                if (cache.contains(info))
                {
                    return cache.at(info);
                }
                else
                {
                    std::vector<vk::DescriptorSetLayout> denseLayouts {};
                    denseLayouts.resize(info.descriptors.size());

                    for (auto& l : info.descriptors)
                    {
                        denseLayouts.push_back(**l);
                    }

                    const vk::PipelineLayoutCreateInfo
                        pipelineLayoutCreateInfo {
                            .sType {
                                vk::StructureType::ePipelineLayoutCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .setLayoutCount {
                                static_cast<U32>(denseLayouts.size())},
                            .pSetLayouts {denseLayouts.data()},
                            .pushConstantRangeCount {
                                info.push_constants.has_value() ? 1U : 0U},
                            .pPushConstantRanges {
                                info.push_constants.has_value()
                                    ? &*info.push_constants
                                    : nullptr},
                        };

                    vk::UniquePipelineLayout layout =
                        this->device.createPipelineLayoutUnique(
                            pipelineLayoutCreateInfo);

                    std::shared_ptr<vk::UniquePipelineLayout> // NOLINT
                        sharedLayout {
                            new vk::UniquePipelineLayout {std::move(layout)}};

                    cache[info] = sharedLayout;

                    return sharedLayout;
                }
            });
    }

    std::shared_ptr<vk::UniquePipeline>
    Allocator::cachePipeline(CacheableGraphicsPipelineCreateInfo info) const
    {
        return this->graphics_pipeline_cache.lock(
            [&](auto& cache)
            {
                if (cache.contains(info))
                {
                    return cache.at(info);
                }
                else
                {
                    std::vector<vk::PipelineShaderStageCreateInfo>
                        denseStages {};
                    denseStages.reserve(info.stages.size());

                    for (const CacheablePipelineShaderStageCreateInfo& s :
                         info.stages)
                    {
                        denseStages.push_back(
                            vk::PipelineShaderStageCreateInfo {

                                .sType {vk::StructureType::
                                            ePipelineShaderStageCreateInfo},
                                .pNext {nullptr},
                                .flags {},
                                .stage {s.stage},
                                .module {**s.shader},
                                .pName {s.entry_point.c_str()},
                                .pSpecializationInfo {},
                            });
                    }

                    const vk::PipelineVertexInputStateCreateInfo
                        pipelineVertexInputStateCreateInfo {
                            .sType {vk::StructureType::
                                        ePipelineVertexInputStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .vertexBindingDescriptionCount {
                                static_cast<U32>(info.vertex_bindings.size())},
                            .pVertexBindingDescriptions {
                                info.vertex_bindings.data()},
                            .vertexAttributeDescriptionCount {static_cast<U32>(
                                info.vertex_attributes.size())},
                            .pVertexAttributeDescriptions {
                                info.vertex_attributes.data()},
                        };

                    const vk::PipelineInputAssemblyStateCreateInfo
                        pipelineInputAssemblyStateCreateInfo {
                            .sType {vk::StructureType::
                                        ePipelineInputAssemblyStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .topology {info.topology},
                            .primitiveRestartEnable {false},
                        };

                    const vk::GraphicsPipelineCreateInfo pipelineCreateInfo {
                        .sType {vk::StructureType::eGraphicsPipelineCreateInfo},
                        .pNext {nullptr},
                        .flags {info.pipeline_flags},
                        .stageCount {static_cast<U32>(denseStages.size())},
                        .pStages {denseStages.data()},
                        .pVertexInputState {
                            &pipelineVertexInputStateCreateInfo},
                        .pInputAssemblyState {
                            &pipelineInputAssemblyStateCreateInfo},
                        .pTessellationState {nullptr},
                        .pViewportState {},
                        .pRasterizationState {},
                        .pMultisampleState {},
                        .pDepthStencilState {},
                        .pColorBlendState {},
                        .pDynamicState {},
                        .layout {},
                        .renderPass {},
                        .subpass {},
                        .basePipelineHandle {},
                        .basePipelineIndex {},
                    };

                    auto [result, pipelines] =
                        this->device.createGraphicsPipelinesUnique(
                            nullptr, pipelineCreateInfo);

                    util::assertFatal(
                        result == vk::Result::eSuccess,
                        "Graphics Pipeline Construction Failed! | {}",
                        vk::to_string(result));

                    vk::UniquePipeline pipeline = std::move(pipelines.at(0));

                    std::shared_ptr<vk::UniquePipeline> // NOLINT
                        sharedPipeline {
                            new vk::UniquePipeline {std::move(pipeline)}};

                    cache[info] = sharedPipeline;

                    return sharedPipeline;
                }
            });
    }
    // std::shared_ptr<vk::UniquePipelineLayout>
    //     cachePipelineLayout(CacheablePipelineLayoutCreateInfo) const;
    // std::shared_ptr<vk::UniquePipeline>
    //     cachePipeline(CacheableGraphicsPipelineCreateInfo) const;
    // std::shared_ptr<vk::UniqueShaderModule>
    //     cacheShaderModule(std::span<const std::byte>);

} // namespace gfx::vulkan