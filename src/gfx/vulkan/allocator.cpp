#include "allocator.hpp"
#include "device.hpp"
#include "instance.hpp"
#include <memory>
#include <string>
#include <util/log.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    Allocator::Allocator(const Instance& instance, const Device* device_)
        : device {device_}
        , allocator {nullptr}
    {
        VmaVulkanFunctions vulkanFunctions {};
        vulkanFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr   = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

        const VmaAllocatorCreateInfo allocatorCreateInfo {
            .flags {},
            .physicalDevice {device_->getPhysicalDevice()},
            .device {this->device->getDevice()},
            .preferredLargeHeapBlockSize {0}, // chosen by VMA
            .pAllocationCallbacks {nullptr},
            .pDeviceMemoryCallbacks {nullptr},
            .pHeapSizeLimit {nullptr},
            .pVulkanFunctions {&vulkanFunctions},
            .instance {*instance},
            .vulkanApiVersion {instance.getVulkanVersion()},
            .pTypeExternalMemoryHandleTypes {nullptr},
        };

        const vk::Result result {::vmaCreateAllocator(&allocatorCreateInfo, &this->allocator)};

        util::assertFatal(
            result == vk::Result::eSuccess,
            "Failed to create allocator | {}",
            vk::to_string(result));

        const std::array availableDescriptors {
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eSampler}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eCombinedImageSampler}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eSampledImage}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eStorageImage}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eUniformTexelBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eStorageTexelBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eUniformBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eStorageBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eUniformBufferDynamic}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eStorageBufferDynamic}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {
                .type {vk::DescriptorType::eInputAttachment}, .descriptorCount {1024}}};

        const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {
            .sType {vk::StructureType::eDescriptorPoolCreateInfo},
            .pNext {nullptr},
            .flags {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet},
            .maxSets {4096},
            .poolSizeCount {static_cast<u32>(availableDescriptors.size())},
            .pPoolSizes {availableDescriptors.data()},
        };

        this->descriptor_pool =
            this->device->getDevice().createDescriptorPoolUnique(descriptorPoolCreateInfo);

        if constexpr (util::isDebugBuild())
        {
            this->device->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eDescriptorPool},
                .objectHandle {std::bit_cast<u64>(*this->descriptor_pool)},
                .pObjectName {"Global Descriptor Pool"},
            });
        }

        this->pipeline_cache =
            this->device->getDevice().createPipelineCacheUnique(vk::PipelineCacheCreateInfo {
                .sType {vk::StructureType::ePipelineCacheCreateInfo},
                .pNext {nullptr},
                .flags {},
                .initialDataSize {0},
                .pInitialData {nullptr}});

        util::logTrace("Created allocator");
    }

    Allocator::~Allocator()
    {
        ::vmaDestroyAllocator(this->allocator);
    }

    VmaAllocator Allocator::operator* () const
    {
        return this->allocator;
    }

    void Allocator::trimCaches() const // NOLINT
    {
        this->descriptor_set_layout_cache.lock(
            [](std::unordered_map<
                gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo,
                std::shared_ptr<vk::UniqueDescriptorSetLayout>>& cache)
            {
                std::vector<gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo>
                    createInfosToRemove {};

                for (const auto& [info, ptr] : cache)
                {
                    if (ptr.use_count() == 1)
                    {
                        createInfosToRemove.push_back(info);
                    }
                }

                for (const gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo& i :
                     createInfosToRemove)
                {
                    cache.erase(i);
                }
            });

        this->pipeline_layout_cache.lock(
            [](std::unordered_map<
                gfx::vulkan::CacheablePipelineLayoutCreateInfo,
                std::shared_ptr<vk::UniquePipelineLayout>>& cache)
            {
                std::vector<gfx::vulkan::CacheablePipelineLayoutCreateInfo> createInfosToRemove {};

                for (const auto& [info, ptr] : cache)
                {
                    if (ptr.use_count() == 1)
                    {
                        createInfosToRemove.push_back(info);
                    }
                }

                for (const gfx::vulkan::CacheablePipelineLayoutCreateInfo& i : createInfosToRemove)
                {
                    cache.erase(i);
                }
            });

        this->pipeline_layout_and_bind_lookup.lock(
            [](std::unordered_map<
                vk::Pipeline,
                std::pair<std::weak_ptr<vk::UniquePipelineLayout>, vk::PipelineBindPoint>>& cache)
            {
                std::vector<vk::Pipeline> pipelinesToRemove {};

                for (const auto& [pipeline, ptr] : cache)
                {
                    if (ptr.first.expired())
                    {
                        pipelinesToRemove.push_back(pipeline);
                    }
                }

                for (const vk::Pipeline& i : pipelinesToRemove)
                {
                    cache.erase(i);
                }
            });

        this->graphics_pipeline_cache.lock(
            [](std::unordered_map<
                gfx::vulkan::CacheableGraphicsPipelineCreateInfo,
                std::shared_ptr<vk::UniquePipeline>>& cache)
            {
                std::vector<gfx::vulkan::CacheableGraphicsPipelineCreateInfo>
                    createInfosToRemove {};

                for (const auto& [info, ptr] : cache)
                {
                    if (ptr.use_count() == 1)
                    {
                        createInfosToRemove.push_back(info);
                    }
                }

                for (const gfx::vulkan::CacheableGraphicsPipelineCreateInfo& i :
                     createInfosToRemove)
                {
                    cache.erase(i);
                }
            });

        this->compute_pipeline_cache.lock(
            [](std::unordered_map<
                gfx::vulkan::CacheableComputePipelineCreateInfo,
                std::shared_ptr<vk::UniquePipeline>>& cache)
            {
                std::vector<gfx::vulkan::CacheableComputePipelineCreateInfo> createInfosToRemove {};

                for (const auto& [info, ptr] : cache)
                {
                    if (ptr.use_count() == 1)
                    {
                        createInfosToRemove.push_back(info);
                    }
                }

                for (const gfx::vulkan::CacheableComputePipelineCreateInfo& i : createInfosToRemove)
                {
                    cache.erase(i);
                }
            });

        this->shader_module_cache.lock(
            [](std::unordered_map<std::string, std::shared_ptr<vk::UniqueShaderModule>>& cache)
            {
                std::vector<std::string> createInfosToRemove {};

                for (const auto& [info, ptr] : cache)
                {
                    if (ptr.use_count() == 1)
                    {
                        createInfosToRemove.push_back(info);
                    }
                }

                for (const std::string& i : createInfosToRemove)
                {
                    cache.erase(i);
                }
            });
    }

    vk::DescriptorSet Allocator::allocateDescriptorSet(
        vk::DescriptorSetLayout layout, const std::string& debugName) const
    {
        const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {
            .sType {vk::StructureType::eDescriptorSetAllocateInfo},
            .pNext {nullptr},
            .descriptorPool {*this->descriptor_pool},
            .descriptorSetCount {1},
            .pSetLayouts {&layout},
        };

        vk::DescriptorSet set =
            this->device->getDevice().allocateDescriptorSets(descriptorSetAllocateInfo).at(0);

        if constexpr (util::isDebugBuild())
        {
            this->device->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eDescriptorSet},
                .objectHandle {std::bit_cast<u64>(set)},
                .pObjectName {debugName.c_str()},
            });
        }

        return set;
    }

    void Allocator::earlyDeallocateDescriptorSet(vk::DescriptorSet set) const
    {
        this->device->getDevice().freeDescriptorSets(*this->descriptor_pool, {set});
    }

    std::shared_ptr<vk::UniqueDescriptorSetLayout>
    Allocator::cacheDescriptorSetLayout(CacheableDescriptorSetLayoutCreateInfo info) const
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
                    const vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {

                        .sType {vk::StructureType::eDescriptorSetLayoutCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .bindingCount {static_cast<u32>(info.bindings.size())},
                        .pBindings {info.bindings.data()},

                    };

                    vk::UniqueDescriptorSetLayout layout =
                        this->device->getDevice().createDescriptorSetLayoutUnique(
                            descriptorSetLayoutCreateInfo);

                    if constexpr (util::isDebugBuild())
                    {
                        this->device->getDevice().setDebugUtilsObjectNameEXT(
                            vk::DebugUtilsObjectNameInfoEXT {
                                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                                .pNext {nullptr},
                                .objectType {vk::ObjectType::eDescriptorSetLayout},
                                .objectHandle {std::bit_cast<u64>(*layout)},
                                .pObjectName {info.name.c_str()},
                            });
                    }

                    std::shared_ptr<vk::UniqueDescriptorSetLayout> // NOLINT
                        sharedLayout {new vk::UniqueDescriptorSetLayout {std::move(layout)}};

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
                    denseLayouts.reserve(info.descriptors.size());

                    for (auto& l : info.descriptors)
                    {
                        denseLayouts.push_back(**l);
                    }

                    const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
                        .sType {vk::StructureType::ePipelineLayoutCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .setLayoutCount {static_cast<u32>(denseLayouts.size())},
                        .pSetLayouts {denseLayouts.data()},
                        .pushConstantRangeCount {info.push_constants.has_value() ? 1U : 0U},
                        .pPushConstantRanges {
                            info.push_constants.has_value() ? &*info.push_constants : nullptr},
                    };

                    vk::UniquePipelineLayout layout =
                        this->device->getDevice().createPipelineLayoutUnique(
                            pipelineLayoutCreateInfo);

                    if constexpr (util::isDebugBuild())
                    {
                        this->device->getDevice().setDebugUtilsObjectNameEXT(
                            vk::DebugUtilsObjectNameInfoEXT {
                                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                                .pNext {nullptr},
                                .objectType {vk::ObjectType::ePipelineLayout},
                                .objectHandle {std::bit_cast<u64>(*layout)},
                                .pObjectName {info.name.c_str()},
                            });
                    }

                    std::shared_ptr<vk::UniquePipelineLayout> // NOLINT
                        sharedLayout {new vk::UniquePipelineLayout {std::move(layout)}};

                    cache[info] = sharedLayout;

                    return sharedLayout;
                }
            });
    }

    std::shared_ptr<vk::UniquePipeline>
    Allocator::cachePipeline(CacheableComputePipelineCreateInfo info) const
    {
        return this->compute_pipeline_cache.lock(
            [&](auto& cache)
            {
                if (cache.contains(info))
                {
                    return cache.at(info);
                }
                else
                {
                    const vk::PipelineShaderStageCreateInfo shaderCreateInfo {
                        .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .stage {vk::ShaderStageFlagBits::eCompute},
                        .module {**info.shader},
                        .pName {info.entry_point.c_str()},
                        .pSpecializationInfo {},
                    };

                    const vk::ComputePipelineCreateInfo computePipelineCreateInfo {
                        .sType {vk::StructureType::eComputePipelineCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .stage {shaderCreateInfo},
                        .layout {**info.layout},
                        .basePipelineHandle {},
                        .basePipelineIndex {},
                    };

                    auto [result, pipeline] = this->device->getDevice().createComputePipelineUnique(
                        *this->pipeline_cache, computePipelineCreateInfo);

                    util::assertFatal(
                        result == vk::Result::eSuccess,
                        "Compute Pipeline Construction Failed! | {}",
                        vk::to_string(result));

                    if constexpr (util::isDebugBuild())
                    {
                        this->device->getDevice().setDebugUtilsObjectNameEXT(
                            vk::DebugUtilsObjectNameInfoEXT {
                                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                                .pNext {nullptr},
                                .objectType {vk::ObjectType::ePipeline},
                                .objectHandle {std::bit_cast<u64>(*pipeline)},
                                .pObjectName {info.name.c_str()},
                            });
                    }

                    std::shared_ptr<vk::UniquePipeline> // NOLINT
                        sharedPipeline {new vk::UniquePipeline {std::move(pipeline)}};

                    this->pipeline_layout_and_bind_lookup.lock(
                        [&](std::unordered_map<
                            vk::Pipeline,
                            std::pair<
                                std::weak_ptr<vk::UniquePipelineLayout>,
                                vk::PipelineBindPoint>>& lookup)
                        {
                            lookup[**sharedPipeline] = {
                                info.layout, vk::PipelineBindPoint::eCompute};
                        });

                    cache[info] = sharedPipeline;

                    return sharedPipeline;
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
                    std::vector<vk::PipelineShaderStageCreateInfo> denseStages {};
                    denseStages.reserve(info.stages.size());

                    for (const CacheablePipelineShaderStageCreateInfo& s : info.stages)
                    {
                        denseStages.push_back(vk::PipelineShaderStageCreateInfo {

                            .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .stage {s.stage},
                            .module {**s.shader},
                            .pName {s.entry_point.c_str()},
                            .pSpecializationInfo {},
                        });
                    }

                    util::assertFatal(
                        !denseStages.empty(), "All pipelines must have at least one shader!");

                    const vk::PipelineVertexInputStateCreateInfo
                        pipelineVertexInputStateCreateInfo {
                            .sType {vk::StructureType::ePipelineVertexInputStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .vertexBindingDescriptionCount {
                                static_cast<u32>(info.vertex_bindings.size())},
                            .pVertexBindingDescriptions {info.vertex_bindings.data()},
                            .vertexAttributeDescriptionCount {
                                static_cast<u32>(info.vertex_attributes.size())},
                            .pVertexAttributeDescriptions {info.vertex_attributes.data()},
                        };

                    const vk::PipelineInputAssemblyStateCreateInfo
                        pipelineInputAssemblyStateCreateInfo {
                            .sType {vk::StructureType::ePipelineInputAssemblyStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .topology {info.topology},
                            .primitiveRestartEnable {false},
                        };

                    const vk::Viewport nullDynamicViewport {};
                    const vk::Rect2D   nullDynamicScissor {};

                    const vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo {
                        .sType {vk::StructureType::ePipelineViewportStateCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .viewportCount {1},
                        .pViewports {&nullDynamicViewport},
                        .scissorCount {1},
                        .pScissors {&nullDynamicScissor},
                    };

                    const vk::PipelineRasterizationStateCreateInfo
                        pipelineRasterizationStateCreateInfo {
                            .sType {vk::StructureType::ePipelineRasterizationStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .depthClampEnable {vk::False},
                            .rasterizerDiscardEnable {info.discard_enable},
                            .polygonMode {info.polygon_mode},
                            .cullMode {info.cull_mode},
                            .frontFace {info.front_face},
                            .depthBiasEnable {vk::False},
                            .depthBiasConstantFactor {0.0},
                            .depthBiasClamp {0.0},
                            .depthBiasSlopeFactor {0.0},
                            .lineWidth {1.0},
                        };

                    const vk::PipelineMultisampleStateCreateInfo
                        pipelineMultisampleStateCreateInfo {
                            .sType {vk::StructureType::ePipelineMultisampleStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .rasterizationSamples {vk::SampleCountFlagBits::e1},
                            .sampleShadingEnable {vk::False},
                            .minSampleShading {1.0},
                            .pSampleMask {nullptr},
                            .alphaToCoverageEnable {vk::False},
                            .alphaToOneEnable {vk::False},
                        };

                    const vk::PipelineDepthStencilStateCreateInfo
                        pipelineDepthStencilStateCreateInfo {
                            .sType {vk::StructureType::ePipelineDepthStencilStateCreateInfo},
                            .pNext {nullptr},
                            .flags {},
                            .depthTestEnable {info.depth_test_enable},
                            .depthWriteEnable {info.depth_write_enable},
                            .depthCompareOp {info.depth_compare_op},
                            .depthBoundsTestEnable {vk::False}, // TODO: expose?
                            .stencilTestEnable {vk::False},
                            .front {},
                            .back {},
                            .minDepthBounds {0.0}, // TODO: expose?
                            .maxDepthBounds {1.0}, // TODO: expose?
                        };

                    const vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {
                        .blendEnable {info.blend_enable},
                        .srcColorBlendFactor {vk::BlendFactor::eSrcAlpha},
                        .dstColorBlendFactor {vk::BlendFactor::eOneMinusSrcAlpha},
                        .colorBlendOp {vk::BlendOp::eAdd},
                        .srcAlphaBlendFactor {vk::BlendFactor::eOne},
                        .dstAlphaBlendFactor {vk::BlendFactor::eZero},
                        .alphaBlendOp {vk::BlendOp::eAdd},
                        .colorWriteMask {
                            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA},
                    };

                    const vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo {
                        .sType {vk::StructureType::ePipelineColorBlendStateCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .logicOpEnable {vk::False},
                        .logicOp {vk::LogicOp::eCopy},
                        .attachmentCount {1},
                        .pAttachments {&pipelineColorBlendAttachmentState},
                        .blendConstants {{0.0, 0.0, 0.0, 0.0}},
                    };

                    const std::array pipelineDynamicStates {
                        vk::DynamicState::eScissor,
                        vk::DynamicState::eViewport,
                    };

                    const vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {
                        .sType {vk::StructureType::ePipelineDynamicStateCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .dynamicStateCount {static_cast<u32>(pipelineDynamicStates.size())},
                        .pDynamicStates {pipelineDynamicStates.data()},
                    };

                    const vk::PipelineRenderingCreateInfo renderingInfo {
                        .sType {vk::StructureType::ePipelineRenderingCreateInfo},
                        .pNext {nullptr},
                        .viewMask {0},
                        .colorAttachmentCount {
                            info.color_format == vk::Format::eUndefined ? 0u : 1u},
                        .pColorAttachmentFormats {&info.color_format},
                        .depthAttachmentFormat {info.depth_format},
                        .stencilAttachmentFormat {},
                    };

                    const vk::GraphicsPipelineCreateInfo pipelineCreateInfo {
                        .sType {vk::StructureType::eGraphicsPipelineCreateInfo},
                        .pNext {&renderingInfo},
                        .flags {},
                        .stageCount {static_cast<u32>(denseStages.size())},
                        .pStages {denseStages.data()},
                        .pVertexInputState {&pipelineVertexInputStateCreateInfo},
                        .pInputAssemblyState {&pipelineInputAssemblyStateCreateInfo},
                        .pTessellationState {nullptr},
                        .pViewportState {&pipelineViewportStateCreateInfo},
                        .pRasterizationState {&pipelineRasterizationStateCreateInfo},
                        .pMultisampleState {&pipelineMultisampleStateCreateInfo},
                        .pDepthStencilState {&pipelineDepthStencilStateCreateInfo},
                        .pColorBlendState {&pipelineColorBlendStateCreateInfo},
                        .pDynamicState {&pipelineDynamicStateCreateInfo},
                        .layout {**info.layout},
                        .renderPass {nullptr},
                        .subpass {0},
                        .basePipelineHandle {nullptr},
                        .basePipelineIndex {0},
                    };

                    auto [result, pipeline] =
                        this->device->getDevice().createGraphicsPipelineUnique(
                            *this->pipeline_cache, pipelineCreateInfo);

                    util::assertFatal(
                        result == vk::Result::eSuccess,
                        "Graphics Pipeline Construction Failed! | {}",
                        vk::to_string(result));

                    if constexpr (util::isDebugBuild())
                    {
                        this->device->getDevice().setDebugUtilsObjectNameEXT(
                            vk::DebugUtilsObjectNameInfoEXT {
                                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                                .pNext {nullptr},
                                .objectType {vk::ObjectType::ePipeline},
                                .objectHandle {std::bit_cast<u64>(*pipeline)},
                                .pObjectName {info.name.c_str()},
                            });
                    }

                    std::shared_ptr<vk::UniquePipeline> // NOLINT
                        sharedPipeline {new vk::UniquePipeline {std::move(pipeline)}};

                    this->pipeline_layout_and_bind_lookup.lock(
                        [&](std::unordered_map<
                            vk::Pipeline,
                            std::pair<
                                std::weak_ptr<vk::UniquePipelineLayout>,
                                vk::PipelineBindPoint>>& lookup)
                        {
                            lookup[**sharedPipeline] = {
                                info.layout, vk::PipelineBindPoint::eGraphics};
                        });

                    cache[info] = sharedPipeline;

                    return sharedPipeline;
                }
            });
    }

    std::shared_ptr<vk::UniqueShaderModule>
    Allocator::cacheShaderModule(std::span<const std::byte> shaderCode, std::string debugName) const
    {
        std::string shaderString {// this is fine NOLINTNEXTLINE
                                  reinterpret_cast<const char*>(shaderCode.data()),
                                  shaderCode.size_bytes()};

        return this->shader_module_cache.lock(
            [&](auto& cache)
            {
                if (cache.contains(shaderString))
                {
                    return cache.at(shaderString);
                }
                else
                {
                    util::assertWarn( // this is also fine NOLINTNEXTLINE
                        reinterpret_cast<std::uintptr_t>(shaderCode.data()) % 4 == 0,
                        "shaderCode is underaligned!");

                    const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {

                        .sType {vk::StructureType::eShaderModuleCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .codeSize {shaderString.size()},
                        // this is just straight up a strict aliasing violation
                        // this is not fine, however given a provenance barrier
                        // like std::start_lifetime_as_array + ensuring
                        // alignment means that we're fine on x86 and arm which
                        // is all we care about 😍
                        // NOLINTNEXTLINE
                        .pCode {reinterpret_cast<u32*>(shaderString.data())},
                    };

                    vk::UniqueShaderModule module =
                        this->device->getDevice().createShaderModuleUnique(shaderModuleCreateInfo);

                    if constexpr (util::isDebugBuild())
                    {
                        this->device->getDevice().setDebugUtilsObjectNameEXT(
                            vk::DebugUtilsObjectNameInfoEXT {
                                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                                .pNext {nullptr},
                                .objectType {vk::ObjectType::eShaderModule},
                                .objectHandle {std::bit_cast<u64>(*module)},
                                .pObjectName {debugName.c_str()},
                            });
                    }

                    std::shared_ptr<vk::UniqueShaderModule> // NOLINT
                        sharedModule {new vk::UniqueShaderModule {std::move(module)}};

                    cache[std::move(shaderString)] = sharedModule;

                    return sharedModule;
                }
            });
    }

    std::shared_ptr<vk::UniquePipelineLayout>
    Allocator::lookupPipelineLayout(vk::Pipeline pipeline) const
    {
        return this->pipeline_layout_and_bind_lookup.lock(
            [&](std::unordered_map<
                vk::Pipeline,
                std::pair<std::weak_ptr<vk::UniquePipelineLayout>, vk::PipelineBindPoint>>& cache)
            {
                std::shared_ptr shouldBeLayout = cache.at(pipeline).first.lock();

                util::assertFatal(
                    shouldBeLayout != nullptr,
                    "Tried to lookup pipeline layout of expired pipeline");

                return shouldBeLayout;
            });
    }

    vk::PipelineBindPoint Allocator::lookupPipelineBindPoint(vk::Pipeline pipeline) const
    {
        return this->pipeline_layout_and_bind_lookup.lock(
            [&](std::unordered_map<
                vk::Pipeline,
                std::pair<std::weak_ptr<vk::UniquePipelineLayout>, vk::PipelineBindPoint>>& cache)
            {
                return cache.at(pipeline).second;
            });
    }

    const Device* Allocator::getDevice() const
    {
        return this->device;
    }

    vk::DescriptorPool Allocator::getRawPool() const
    {
        return *this->descriptor_pool;
    }

    vk::PipelineCache Allocator::getRawCache() const
    {
        return *this->pipeline_cache;
    }
} // namespace gfx::vulkan
