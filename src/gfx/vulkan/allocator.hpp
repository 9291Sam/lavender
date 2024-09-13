#pragma once

#include "util/misc.hpp"
#include "util/threads.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hash.hpp>
#include <vulkan/vulkan_structs.hpp>

VK_DEFINE_HANDLE(VmaAllocator)

namespace gfx::vulkan
{

    struct CacheableDescriptorSetLayoutCreateInfo
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        bool operator== (const CacheableDescriptorSetLayoutCreateInfo&) const =
            default;
    };

    struct CacheablePipelineLayoutCreateInfo
    {
        std::vector<std::shared_ptr<vk::UniqueDescriptorSetLayout>> descriptors;
        std::optional<vk::PushConstantRange> push_constants;

        bool
        operator== (const CacheablePipelineLayoutCreateInfo&) const = default;
    };

    struct CacheablePipelineShaderStageCreateInfo
    {
        vk::ShaderStageFlagBits                 stage;
        std::shared_ptr<vk::UniqueShaderModule> shader;
        std::string                             entry_point;

        bool operator== (const CacheablePipelineShaderStageCreateInfo&) const =
            default;
    };

    struct CacheableGraphicsPipelineCreateInfo
    {
        std::vector<CacheablePipelineShaderStageCreateInfo> stages;
        std::vector<vk::VertexInputAttributeDescription>    vertex_attributes;
        std::vector<vk::VertexInputBindingDescription>      vertex_bindings;
        vk::PrimitiveTopology                               topology;
        bool                                                discard_enable;
        vk::PolygonMode                                     polygon_mode;
        vk::CullModeFlags                                   cull_mode;
        vk::FrontFace                                       front_face;
        bool                                                depth_test_enable;
        bool                                                depth_write_enable;
        vk::CompareOp                                       depth_compare_op;
        vk::Format                                          color_format;
        vk::Format                                          depth_format;
        std::shared_ptr<vk::UniquePipelineLayout>           layout;

        bool
        operator== (const CacheableGraphicsPipelineCreateInfo&) const = default;
    };
} // namespace gfx::vulkan

template<>
struct std::hash<gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo>
{
    std::size_t
    operator() (const gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo& i)
        const noexcept
    {
        std::size_t result = 1394897243;

        for (const vk::DescriptorSetLayoutBinding& b : i.bindings)
        {
            util::hashCombine(
                result, std::hash<vk::DescriptorSetLayoutBinding> {}(b));
        }

        return result;
    }
};

template<>
struct std::hash<gfx::vulkan::CacheablePipelineLayoutCreateInfo>
{
    std::size_t operator() (
        const gfx::vulkan::CacheablePipelineLayoutCreateInfo& i) const noexcept
    {
        std::size_t result = 5783547893548971;

        for (const std::shared_ptr<vk::UniqueDescriptorSetLayout>& d :
             i.descriptors)
        {
            util::hashCombine(
                result, static_cast<std::size_t>(std::bit_cast<U64>(**d)));
        }

        util::hashCombine(
            result,
            std::hash<std::optional<vk::PushConstantRange>> {}(
                i.push_constants));

        return result;
    }
};

template<>
struct std::hash<gfx::vulkan::CacheablePipelineShaderStageCreateInfo>
{
    std::size_t
    operator() (const gfx::vulkan::CacheablePipelineShaderStageCreateInfo& i)
        const noexcept
    {
        std::size_t result = 5783547893548971;

        util::hashCombine(result, std::hash<std::string> {}(i.entry_point));

        util::hashCombine(
            result, static_cast<std::size_t>(std::bit_cast<U64>(**i.shader)));

        util::hashCombine(
            result, std::hash<vk::ShaderStageFlagBits> {}(i.stage));

        return result;
    }
};

template<>
struct std::hash<gfx::vulkan::CacheableGraphicsPipelineCreateInfo>
{
    std::size_t
    operator() (const gfx::vulkan::CacheableGraphicsPipelineCreateInfo& i)
        const noexcept
    {
        std::size_t result = 5783547893548971;

        for (const gfx::vulkan::CacheablePipelineShaderStageCreateInfo& d :
             i.stages)
        {
            util::hashCombine(
                result,
                std::hash<
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo> {}(d));
        }

        for (const vk::VertexInputAttributeDescription& d : i.vertex_attributes)
        {
            util::hashCombine(
                result, std::hash<vk::VertexInputAttributeDescription> {}(d));
        }

        for (const vk::VertexInputBindingDescription& d : i.vertex_bindings)
        {
            util::hashCombine(
                result, std::hash<vk::VertexInputBindingDescription> {}(d));
        }

        util::hashCombine(
            result, std::hash<vk::PrimitiveTopology> {}(i.topology));
        util::hashCombine(result, std::hash<bool> {}(i.discard_enable));
        util::hashCombine(
            result, std::hash<vk::PolygonMode> {}(i.polygon_mode));
        util::hashCombine(result, std::hash<vk::CullModeFlags> {}(i.cull_mode));
        util::hashCombine(result, std::hash<vk::FrontFace> {}(i.front_face));
        util::hashCombine(result, std::hash<bool> {}(i.depth_test_enable));
        util::hashCombine(result, std::hash<bool> {}(i.depth_write_enable));
        util::hashCombine(
            result, std::hash<vk::CompareOp> {}(i.depth_compare_op));
        util::hashCombine(result, std::hash<vk::Format> {}(i.color_format));
        util::hashCombine(result, std::hash<vk::Format> {}(i.depth_format));
        util::hashCombine(
            result, static_cast<std::size_t>(std::bit_cast<U64>(**i.layout)));

        return result;
    }
};

namespace gfx::vulkan
{
    class Instance;
    class Device;
    class DescriptorPool;
    class PipelineCache;

    class Allocator
    {
    public:

        Allocator(const Instance&, const Device&);
        ~Allocator();

        Allocator(const Allocator&)             = delete;
        Allocator(Allocator&&)                  = delete;
        Allocator& operator= (const Allocator&) = delete;
        Allocator& operator= (Allocator&&)      = delete;

        [[nodiscard]] VmaAllocator operator* () const;

        void trimCaches() const;

        [[nodiscard]] vk::DescriptorSet
             allocateDescriptorSet(vk::DescriptorSetLayout) const;
        void earlyDeallocateDescriptorSet(vk::DescriptorSet) const;

        [[nodiscard]] std::shared_ptr<vk::UniqueDescriptorSetLayout>
            cacheDescriptorSetLayout(
                CacheableDescriptorSetLayoutCreateInfo) const;
        [[nodiscard]] std::shared_ptr<vk::UniquePipelineLayout>
            cachePipelineLayout(CacheablePipelineLayoutCreateInfo) const;
        [[nodiscard]] std::shared_ptr<vk::UniquePipeline>
            cachePipeline(CacheableGraphicsPipelineCreateInfo) const;
        [[nodiscard]] std::shared_ptr<vk::UniqueShaderModule>
            cacheShaderModule(std::span<const std::byte>) const;

    private:
        vk::Device               device;
        VmaAllocator             allocator;
        vk::UniqueDescriptorPool descriptor_pool;
        util::Mutex<std::unordered_map<
            CacheableDescriptorSetLayoutCreateInfo,
            std::shared_ptr<vk::UniqueDescriptorSetLayout>>>
            descriptor_set_layout_cache;

        util::Mutex<std::unordered_map<
            CacheablePipelineLayoutCreateInfo,
            std::shared_ptr<vk::UniquePipelineLayout>>>
            pipeline_layout_cache;

        util::Mutex<std::unordered_map<
            CacheableGraphicsPipelineCreateInfo,
            std::shared_ptr<vk::UniquePipeline>>>
            graphics_pipeline_cache;

        util::Mutex<std::unordered_map<
            std::string,
            std::shared_ptr<vk::UniqueShaderModule>>>
            shader_module_cache;
    };

} // namespace gfx::vulkan
