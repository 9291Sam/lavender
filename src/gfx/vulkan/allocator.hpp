#pragma once

#include "util/misc.hpp"
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hash.hpp>
#include <vulkan/vulkan_structs.hpp>

VK_DEFINE_HANDLE(VmaAllocator)

namespace gfx::vulkan
{
    class Instance;
    class Device;
    class DescriptorPool;
    class PipelineCache;

    struct CacheableDescriptorSetLayoutCreateInfo
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
    };

    struct CacheablePipelineLayoutCreateInfo
    {
        std::vector<std::shared_ptr<vk::UniqueDescriptorSetLayout>> descriptors;
        vk::PushConstantRange push_constants;
    };

    struct CacheablePipelineShaderStageCreateInfo
    {
        vk::ShaderStageFlags                    stage;
        std::shared_ptr<vk::UniqueShaderModule> shader;
        std::string                             entry_point;
    };

    struct CacheableGraphicsPipelineCreateInfo
    {
        vk::PipelineCreateFlags                             pipeline_flags;
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
    };

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

        [[nodiscard]] vk::DescriptorSet allocateDescriptorSet() const;
        void                            earlyDeallocateDescriptorSet() const;

        [[nodiscard]] std::shared_ptr<vk::UniqueDescriptorSetLayout>
            cacheDescriptorSetLayout(
                CacheableDescriptorSetLayoutCreateInfo) const;
        [[nodiscard]] std::shared_ptr<vk::UniquePipelineLayout>
            cachePipelineLayout(CacheableGraphicsPipelineCreateInfo) const;
        [[nodiscard]] std::shared_ptr<vk::UniquePipeline>
            cachePipeline(CacheableGraphicsPipelineCreateInfo) const;
        [[nodiscard]] std::shared_ptr<vk::UniqueShaderModule>
            cacheShaderModule(std::span<const std::byte>);

    private:
        VmaAllocator             allocator;
        vk::UniqueDescriptorPool descriptor_pool;
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
            result +=
                (result * std::hash<vk::DescriptorSetLayoutBinding> {}(b));
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
            result += (result * std::bit_cast<U64>(**d));
        }

        result += (std::hash<vk::PushConstantRange> {}(i.push_constants));

        return result;
    }
};
