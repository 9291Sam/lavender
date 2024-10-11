#include "frame_generator.hpp"
#include "game.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/vulkan/image.hpp"
#include "util/misc.hpp"
#include <compare>
#include <cstddef>
#include <gfx/renderer.hpp>
#include <gfx/vulkan/swapchain.hpp>
#include <gfx/window.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game
{
    FrameGenerator::GlobalInfoDescriptors FrameGenerator::makeGlobalDescriptors(
        const gfx::Renderer* renderer, vk::DescriptorSet globalDescriptorSet)
    {
        gfx::vulkan::Image2D depthBuffer {
            renderer->getAllocator(),
            renderer->getDevice()->getDevice(),
            renderer->getWindow()->getFramebufferSize(),
            vk::Format::eD32Sfloat,
            vk::ImageLayout::eUndefined,
            vk::ImageUsageFlagBits::eDepthStencilAttachment
                | vk::ImageUsageFlagBits::eSampled,
            vk::ImageAspectFlagBits::eDepth,
            vk::ImageTiling::eOptimal,
            vk::MemoryPropertyFlagBits::eDeviceLocal};

        gfx::vulkan::Buffer<glm::mat4> mvpMatrices {
            renderer->getAllocator(),
            vk::BufferUsageFlagBits::eUniformBuffer
                | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
                | vk::MemoryPropertyFlagBits::eHostVisible,
            1024};

        gfx::vulkan::Buffer<GlobalFrameInfo> globalFrameInfo {
            renderer->getAllocator(),
            vk::BufferUsageFlagBits::eUniformBuffer
                | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
                | vk::MemoryPropertyFlagBits::eHostVisible,
            1};

        const vk::DescriptorBufferInfo mvpMatricesBufferInfo {
            .buffer {*mvpMatrices}, .offset {0}, .range {vk::WholeSize}};

        const vk::DescriptorBufferInfo globalFrameBufferInfo {
            .buffer {*globalFrameInfo}, .offset {0}, .range {vk::WholeSize}};

        gfx::vulkan::Image2D visibleVoxelImage {
            renderer->getAllocator(),
            renderer->getDevice()->getDevice(),
            renderer->getWindow()->getFramebufferSize(),
            vk::Format::eR32Uint,
            vk::ImageLayout::eUndefined,
            vk::ImageUsageFlagBits::eColorAttachment
                | vk::ImageUsageFlagBits::eSampled,
            vk::ImageAspectFlagBits::eColor,
            vk::ImageTiling::eOptimal,
            vk::MemoryPropertyFlagBits::eDeviceLocal};

        gfx::vulkan::Image2D faceIdImage {
            renderer->getAllocator(),
            renderer->getDevice()->getDevice(),
            renderer->getWindow()->getFramebufferSize(),
            vk::Format::eR32Uint,
            vk::ImageLayout::eUndefined,
            vk::ImageUsageFlagBits::eColorAttachment
                | vk::ImageUsageFlagBits::eSampled,
            vk::ImageAspectFlagBits::eColor,
            vk::ImageTiling::eOptimal,
            vk::MemoryPropertyFlagBits::eDeviceLocal};

        vk::UniqueSampler doNothingSampler =
            renderer->getDevice()->getDevice().createSamplerUnique(
                vk::SamplerCreateInfo {
                    .sType {vk::StructureType::eSamplerCreateInfo},
                    .pNext {nullptr},
                    .flags {},
                    .magFilter {vk::Filter::eLinear},
                    .minFilter {vk::Filter::eLinear},
                    .mipmapMode {vk::SamplerMipmapMode::eLinear},
                    .addressModeU {vk::SamplerAddressMode::eRepeat},
                    .addressModeV {vk::SamplerAddressMode::eRepeat},
                    .addressModeW {vk::SamplerAddressMode::eRepeat},
                    .mipLodBias {},
                    .anisotropyEnable {vk::False},
                    .maxAnisotropy {1.0f},
                    .compareEnable {vk::False},
                    .compareOp {vk::CompareOp::eNever},
                    .minLod {0},
                    .maxLod {1},
                    .borderColor {vk::BorderColor::eFloatTransparentBlack},
                    .unnormalizedCoordinates {},
                });

        const vk::DescriptorImageInfo depthBufferInfo {
            .sampler {nullptr},
            .imageView {depthBuffer.getView()},
            .imageLayout {vk::ImageLayout::eDepthReadOnlyOptimal},
        };
        const vk::DescriptorImageInfo visibleVoxelImageInfo {
            .sampler {nullptr},
            .imageView {visibleVoxelImage.getView()},
            .imageLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
        };
        const vk::DescriptorImageInfo faceIdImageInfo {
            .sampler {nullptr},
            .imageView {faceIdImage.getView()},
            .imageLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
        };
        const vk::DescriptorImageInfo doNothingSamplerInfo {
            .sampler {*doNothingSampler},
            .imageView {nullptr},
            .imageLayout {},
        };

        renderer->getDevice()->getDevice().updateDescriptorSets(
            {
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {globalDescriptorSet},
                    .dstBinding {0},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eUniformBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&mvpMatricesBufferInfo},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {globalDescriptorSet},
                    .dstBinding {1},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eUniformBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&globalFrameBufferInfo},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {globalDescriptorSet},
                    .dstBinding {2},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eSampledImage},
                    .pImageInfo {&depthBufferInfo},
                    .pBufferInfo {nullptr},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {globalDescriptorSet},
                    .dstBinding {3},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eSampledImage},
                    .pImageInfo {&visibleVoxelImageInfo},
                    .pBufferInfo {nullptr},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {globalDescriptorSet},
                    .dstBinding {4},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eSampledImage},
                    .pImageInfo {&faceIdImageInfo},
                    .pBufferInfo {nullptr},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {globalDescriptorSet},
                    .dstBinding {5},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eSampler},
                    .pImageInfo {&doNothingSamplerInfo},
                    .pBufferInfo {nullptr},
                    .pTexelBufferView {nullptr},
                },
            },
            {});

        return GlobalInfoDescriptors {
            .mvp_matrices {std::move(mvpMatrices)},
            .global_frame_info {std::move(globalFrameInfo)},
            .depth_buffer {std::move(depthBuffer)},
            .visible_voxel_image {std::move(visibleVoxelImage)},
            .face_id_image {std::move(faceIdImage)},
            .do_nothing_sampler {std::move(doNothingSampler)},
        };
    }

    std::strong_ordering
    FrameGenerator::RecordObject::RecordObject::operator<=> (
        const RecordObject& other) const noexcept
    {
        if (util::toUnderlying(this->render_pass)
            != util::toUnderlying(other.render_pass))
        {
            return util::toUnderlying(this->render_pass)
               <=> util::toUnderlying(other.render_pass);
        }

        if (this->pipeline->get() != other.pipeline->get())
        {
            return this->pipeline->get() <=> other.pipeline->get();
        }

        if (this->descriptors != other.descriptors)
        {
            return this->descriptors <=> other.descriptors;
        }

        return std::strong_ordering::equal;
    }

    FrameGenerator::FrameGenerator(const game::Game* game_)
        : game {game_}
        , has_resize_ocurred {true}
        , set_layout {this->game->getRenderer()
                          ->getAllocator()
                          ->cacheDescriptorSetLayout(
                              gfx::vulkan::
                                  CacheableDescriptorSetLayoutCreateInfo {
                                      .bindings {
                                          vk::DescriptorSetLayoutBinding {
                                              .binding {0},
                                              .descriptorType {
                                                  vk::DescriptorType::
                                                      eUniformBuffer},
                                              .descriptorCount {1},
                                              .stageFlags {
                                                  vk::ShaderStageFlagBits::
                                                      eVertex
                                                  | vk::ShaderStageFlagBits::
                                                      eFragment
                                                  | vk::ShaderStageFlagBits::
                                                      eCompute},
                                              .pImmutableSamplers {nullptr},
                                          },
                                          vk::DescriptorSetLayoutBinding {
                                              .binding {1},
                                              .descriptorType {
                                                  vk::DescriptorType::
                                                      eUniformBuffer},
                                              .descriptorCount {1},
                                              .stageFlags {
                                                  vk::ShaderStageFlagBits::
                                                      eVertex
                                                  | vk::ShaderStageFlagBits::
                                                      eFragment
                                                  | vk::ShaderStageFlagBits::
                                                      eCompute},
                                              .pImmutableSamplers {nullptr},
                                          },
                                          vk::DescriptorSetLayoutBinding {
                                              .binding {2},
                                              .descriptorType {
                                                  vk::DescriptorType::
                                                      eSampledImage},
                                              .descriptorCount {1},
                                              .stageFlags {
                                                  vk::ShaderStageFlagBits::
                                                      eVertex
                                                  | vk::ShaderStageFlagBits::
                                                      eFragment
                                                  | vk::ShaderStageFlagBits::
                                                      eCompute},
                                              .pImmutableSamplers {nullptr},
                                          },
                                          vk::DescriptorSetLayoutBinding {
                                              .binding {3},
                                              .descriptorType {
                                                  vk::DescriptorType::
                                                      eSampledImage},
                                              .descriptorCount {1},
                                              .stageFlags {
                                                  vk::ShaderStageFlagBits::
                                                      eVertex
                                                  | vk::ShaderStageFlagBits::
                                                      eFragment
                                                  | vk::ShaderStageFlagBits::
                                                      eCompute},
                                              .pImmutableSamplers {nullptr},
                                          },
                                          vk::DescriptorSetLayoutBinding {
                                              .binding {4},
                                              .descriptorType {
                                                  vk::DescriptorType::
                                                      eSampledImage},
                                              .descriptorCount {1},
                                              .stageFlags {
                                                  vk::ShaderStageFlagBits::
                                                      eVertex
                                                  | vk::ShaderStageFlagBits::
                                                      eFragment
                                                  | vk::ShaderStageFlagBits::
                                                      eCompute},
                                              .pImmutableSamplers {nullptr},
                                          },
                                          vk::DescriptorSetLayoutBinding {
                                              .binding {5},
                                              .descriptorType {
                                                  vk::DescriptorType::eSampler},
                                              .descriptorCount {1},
                                              .stageFlags {
                                                  vk::ShaderStageFlagBits::
                                                      eVertex
                                                  | vk::ShaderStageFlagBits::
                                                      eFragment
                                                  | vk::ShaderStageFlagBits::
                                                      eCompute},
                                              .pImmutableSamplers {nullptr},
                                          },
                                      }})}
        , global_info_descriptor_set {this->game->getRenderer()
                                          ->getAllocator()
                                          ->allocateDescriptorSet(
                                              **this->set_layout)}
        , global_descriptors {makeGlobalDescriptors(
              this->game->getRenderer(), this->global_info_descriptor_set)}
    {}

    void FrameGenerator::internalGenerateFrame(
        vk::CommandBuffer             commandBuffer,
        u32                           swapchainImageIdx,
        const gfx::vulkan::Swapchain& swapchain,
        std::size_t,
        std::span<const FrameGenerator::RecordObject> recordObjects)
    {
        std::array<
            std::vector<std::pair<const FrameGenerator::RecordObject*, u32>>,
            static_cast<std::size_t>(FrameGenerator::DynamicRenderingPass::
                                         DynamicRenderingPassMaxValue)>
            recordablesByPass;

        std::vector<glm::mat4> localMvpMatrices {};
        u32                    nextFreeMvpMatrix = 0;

        for (const FrameGenerator::RecordObject& o : recordObjects)
        {
            if (o.render_pass != DynamicRenderingPass::PreFrameUpdate)
            {
                util::assertFatal(o.pipeline != nullptr, "Nullpipeline");
            }

            u32 thisMatrixId = 0;

            if (o.transform.has_value())
            {
                localMvpMatrices.push_back(this->camera.getPerspectiveMatrix(
                    *this->game, *o.transform));

                thisMatrixId = nextFreeMvpMatrix;

                nextFreeMvpMatrix += 1;

                util::assertFatal(thisMatrixId < 1024, "too many matriices");
            }

            recordablesByPass
                .at(static_cast<std::size_t>(util::toUnderlying(o.render_pass)))
                .push_back({&o, thisMatrixId});
        }

        commandBuffer.updateBuffer(
            *this->global_descriptors.mvp_matrices,
            0,
            localMvpMatrices.size() * sizeof(glm::mat4),
            localMvpMatrices.data());

        GlobalFrameInfo thisFrameInfo {
            .camera_position {camera.getPosition().xyzz()},
            .frame_number {this->game->getRenderer()->getFrameNumber()},
            .time_alive {this->game->getRenderer()->getTimeAlive()}};

        commandBuffer.updateBuffer(
            *this->global_descriptors.global_frame_info,
            0,
            sizeof(GlobalFrameInfo),
            &thisFrameInfo);

        for (std::vector<std::pair<const FrameGenerator::RecordObject*, u32>>&
                 recordVec : recordablesByPass)
        {
            std::sort(
                recordVec.begin(),
                recordVec.end(),
                [](const auto& l, const auto& r)
                {
                    return *l.first < *r.first;
                });
        }

        for (auto& r : recordablesByPass[static_cast<std::size_t>(
                 FrameGenerator::DynamicRenderingPass::PreFrameUpdate)])
        {
            r.first->record_func(commandBuffer, nullptr, 0);
        }

        const vk::Extent2D renderExtent = swapchain.getExtent();

        const vk::Rect2D scissor {
            .offset {vk::Offset2D {.x {0}, .y {0}}}, .extent {renderExtent}};

        const vk::Viewport renderViewport {
            .x {0.0},
            .y {static_cast<F32>(renderExtent.height)},
            .width {static_cast<F32>(renderExtent.width)},
            .height {static_cast<F32>(renderExtent.height) * -1.0f},
            .minDepth {0.0},
            .maxDepth {1.0},
        };

        const gfx::vulkan::Device& device =
            *this->game->getRenderer()->getDevice();

        const vk::Image thisSwapchainImage =
            swapchain.getImages()[swapchainImageIdx];
        const vk::ImageView thisSwapchainImageView =
            swapchain.getViews()[swapchainImageIdx];
        const u32 graphicsQueueIndex =
            device // NOLINT
                .getFamilyOfQueueType(gfx::vulkan::Device::QueueType::Graphics)
                .value();

        vk::Pipeline                     currentlyBoundPipeline = nullptr;
        std::array<vk::DescriptorSet, 4> currentlyBoundDescriptors {
            nullptr, nullptr, nullptr, nullptr};

        auto updateBindings = [&](const RecordObject& o)
        {
            if (currentlyBoundPipeline != **o.pipeline)
            {
                commandBuffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics, **o.pipeline);
            }

            for (u32 i = 0; i < 4; ++i)
            {
                if (currentlyBoundDescriptors[i] != o.descriptors[i]) // NOLINT
                {
                    currentlyBoundDescriptors[i] = o.descriptors[i]; // NOLINT

                    for (u32 j = i + 1; j < 4; ++j)
                    {
                        currentlyBoundDescriptors[i] = nullptr; // NOLINT
                    }

                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        **this->game->getRenderer()
                              ->getAllocator()
                              ->lookupPipelineLayout(**o.pipeline),
                        i,
                        {o.descriptors[i]}, // NOLINT
                        {});
                }
            }
        };

        auto clearBindings = [&]
        {
            currentlyBoundPipeline    = nullptr;
            currentlyBoundDescriptors = {nullptr, nullptr, nullptr, nullptr};
        };

        if (this->has_resize_ocurred)
        {
            const std::span<const vk::Image> swapchainImages =
                swapchain.getImages();

            std::vector<vk::ImageMemoryBarrier> swapchainMemoryBarriers {};
            swapchainMemoryBarriers.reserve(swapchainImages.size());

            for (const vk::Image i : swapchainImages)
            {
                swapchainMemoryBarriers.push_back(vk::ImageMemoryBarrier {
                    .sType {vk::StructureType::eImageMemoryBarrier},
                    .pNext {nullptr},
                    .srcAccessMask {vk::AccessFlagBits::eNone},
                    .dstAccessMask {vk::AccessFlagBits::eNone},
                    .oldLayout {vk::ImageLayout::eUndefined},
                    .newLayout {vk::ImageLayout::ePresentSrcKHR},
                    .srcQueueFamilyIndex {graphicsQueueIndex},
                    .dstQueueFamilyIndex {graphicsQueueIndex},
                    .image {i},
                    .subresourceRange {vk::ImageSubresourceRange {
                        .aspectMask {vk::ImageAspectFlagBits::eColor},
                        .baseMipLevel {0},
                        .levelCount {1},
                        .baseArrayLayer {0},
                        .layerCount {1}}},
                });
            }

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                {},
                {},
                {},
                swapchainMemoryBarriers);

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eEarlyFragmentTests,
                vk::PipelineStageFlagBits::eEarlyFragmentTests,
                {},
                {},
                {},
                {vk::ImageMemoryBarrier {
                    .sType {vk::StructureType::eImageMemoryBarrier},
                    .pNext {nullptr},
                    .srcAccessMask {vk::AccessFlagBits::eNone},
                    .dstAccessMask {
                        vk::AccessFlagBits::eDepthStencilAttachmentRead},
                    .oldLayout {vk::ImageLayout::eUndefined},
                    .newLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                    .srcQueueFamilyIndex {graphicsQueueIndex},
                    .dstQueueFamilyIndex {graphicsQueueIndex},
                    .image {*this->global_descriptors.depth_buffer},
                    .subresourceRange {vk::ImageSubresourceRange {
                        .aspectMask {vk::ImageAspectFlagBits::eDepth},
                        .baseMipLevel {0},
                        .levelCount {1},
                        .baseArrayLayer {0},
                        .layerCount {1}}},
                }});

            // commandBuffer.pipelineBarrier(
            //     vk::PipelineStageFlagBits::eEarlyFragmentTests,
            //     vk::PipelineStageFlagBits::eEarlyFragmentTests,
            //     {},
            //     {},
            //     {},
            //     {vk::ImageMemoryBarrier {
            //         .sType {vk::StructureType::eImageMemoryBarrier},
            //         .pNext {nullptr},
            //         .srcAccessMask {vk::AccessFlagBits::eNone},
            //         .dstAccessMask {vk::AccessFlagBits::eShaderRead},
            //         .oldLayout {vk::ImageLayout::eUndefined},
            //         .newLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
            //         .srcQueueFamilyIndex {graphicsQueueIndex},
            //         .dstQueueFamilyIndex {graphicsQueueIndex},
            //         .image {*this->global_descriptors.visible_voxel_image},
            //         .subresourceRange {vk::ImageSubresourceRange {
            //             .aspectMask {vk::ImageAspectFlagBits::eColor},
            //             .baseMipLevel {0},
            //             .levelCount {1},
            //             .baseArrayLayer {0},
            //             .layerCount {1}}},
            //     }});

            // commandBuffer.pipelineBarrier(
            //     vk::PipelineStageFlagBits::eEarlyFragmentTests,
            //     vk::PipelineStageFlagBits::eEarlyFragmentTests,
            //     {},
            //     {},
            //     {},
            //     {vk::ImageMemoryBarrier {
            //         .sType {vk::StructureType::eImageMemoryBarrier},
            //         .pNext {nullptr},
            //         .srcAccessMask {vk::AccessFlagBits::eNone},
            //         .dstAccessMask {vk::AccessFlagBits::eShaderRead},
            //         .oldLayout {vk::ImageLayout::eUndefined},
            //         .newLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
            //         .srcQueueFamilyIndex {graphicsQueueIndex},
            //         .dstQueueFamilyIndex {graphicsQueueIndex},
            //         .image {*this->global_descriptors.face_id_image},
            //         .subresourceRange {vk::ImageSubresourceRange {
            //             .aspectMask {vk::ImageAspectFlagBits::eColor},
            //             .baseMipLevel {0},
            //             .levelCount {1},
            //             .baseArrayLayer {0},
            //             .layerCount {1}}},
            //     }});
        }

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::DependencyFlags {},
            {},
            {},
            {vk::ImageMemoryBarrier {
                .sType {vk::StructureType::eImageMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eNone},
                .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                .oldLayout {vk::ImageLayout::ePresentSrcKHR},
                .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
                .srcQueueFamilyIndex {graphicsQueueIndex},
                .dstQueueFamilyIndex {graphicsQueueIndex},
                .image {thisSwapchainImage},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eColor},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eEarlyFragmentTests,
            vk::PipelineStageFlagBits::eEarlyFragmentTests,
            vk::DependencyFlags {},
            {},
            {},
            {vk::ImageMemoryBarrier {
                .sType {vk::StructureType::eImageMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {
                    vk::AccessFlagBits::eDepthStencilAttachmentRead},
                .dstAccessMask {
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite},
                .oldLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                .newLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                .srcQueueFamilyIndex {graphicsQueueIndex},
                .dstQueueFamilyIndex {graphicsQueueIndex},
                .image {*this->global_descriptors.depth_buffer},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eDepth},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});

        // commandBuffer.pipelineBarrier(
        //     vk::PipelineStageFlagBits::eFragmentShader,
        //     vk::PipelineStageFlagBits::eFragmentShader,
        //     vk::DependencyFlags {},
        //     {},
        //     {},
        //     {vk::ImageMemoryBarrier {
        //         .sType {vk::StructureType::eImageMemoryBarrier},
        //         .pNext {nullptr},
        //         .srcAccessMask {vk::AccessFlagBits::eShaderRead},
        //         .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
        //         .oldLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
        //         .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
        //         .srcQueueFamilyIndex {graphicsQueueIndex},
        //         .dstQueueFamilyIndex {graphicsQueueIndex},
        //         .image {*this->global_descriptors.visible_voxel_image},
        //         .subresourceRange {vk::ImageSubresourceRange {
        //             .aspectMask {vk::ImageAspectFlagBits::eColor},
        //             .baseMipLevel {0},
        //             .levelCount {1},
        //             .baseArrayLayer {0},
        //             .layerCount {1}}},
        //     }});

        commandBuffer.setViewport(0, {renderViewport});
        commandBuffer.setScissor(0, {scissor});

        // Voxel Render
        {}

        // barrier on depth buffer

        // barrier on visible voxel image to make it readable
        // commandBuffer.pipelineBarrier(
        //     vk::PipelineStageFlagBits::eFragmentShader,
        //     vk::PipelineStageFlagBits::eFragmentShader,
        //     vk::DependencyFlags {},
        //     {},
        //     {},
        //     {vk::ImageMemoryBarrier {
        //         .sType {vk::StructureType::eImageMemoryBarrier},
        //         .pNext {nullptr},
        //         .srcAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
        //         .dstAccessMask {vk::AccessFlagBits::eShaderRead},
        //         .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
        //         .newLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
        //         .srcQueueFamilyIndex {graphicsQueueIndex},
        //         .dstQueueFamilyIndex {graphicsQueueIndex},
        //         .image {*this->global_descriptors.visible_voxel_image},
        //         .subresourceRange {vk::ImageSubresourceRange {
        //             .aspectMask {vk::ImageAspectFlagBits::eColor},
        //             .baseMipLevel {0},
        //             .levelCount {1},
        //             .baseArrayLayer {0},
        //             .layerCount {1}}},
        //     }});

        // barrier on face id image to make it writable
        // commandBuffer.pipelineBarrier(
        //     vk::PipelineStageFlagBits::eFragmentShader,
        //     vk::PipelineStageFlagBits::eFragmentShader,
        //     vk::DependencyFlags {},
        //     {},
        //     {},
        //     {vk::ImageMemoryBarrier {
        //         .sType {vk::StructureType::eImageMemoryBarrier},
        //         .pNext {nullptr},
        //         .srcAccessMask {vk::AccessFlagBits::eShaderRead},
        //         .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
        //         .oldLayout {vk::ImageLayout::eShaderReadOnlyOptimal},
        //         .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
        //         .srcQueueFamilyIndex {graphicsQueueIndex},
        //         .dstQueueFamilyIndex {graphicsQueueIndex},
        //         .image {*this->global_descriptors.face_id_image},
        //         .subresourceRange {vk::ImageSubresourceRange {
        //             .aspectMask {vk::ImageAspectFlagBits::eColor},
        //             .baseMipLevel {0},
        //             .levelCount {1},
        //             .baseArrayLayer {0},
        //             .layerCount {1}}},
        //     }});

        // Visibility Detection
        {}

        // barrier on faceid image to make it readable

        // Color Calculation
        {}

        // barrier on storage writes before transfer

        // VoxelColorTransfer
        {
            // clear image
        }

        // Simple Color
        const vk::RenderingAttachmentInfo colorAttachmentInfo {
            .sType {vk::StructureType::eRenderingAttachmentInfo},
            .pNext {nullptr},
            .imageView {thisSwapchainImageView},
            .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
            .resolveMode {vk::ResolveModeFlagBits::eNone},
            .resolveImageView {nullptr},
            .resolveImageLayout {},
            .loadOp {vk::AttachmentLoadOp::eClear},
            .storeOp {vk::AttachmentStoreOp::eStore},
            .clearValue {
                vk::ClearColorValue {.float32 {{0.709f, 0.494f, 0.862f, 1.0f}}},
            },
        };

        const vk::RenderingAttachmentInfo depthAttachmentInfo {
            .sType {vk::StructureType::eRenderingAttachmentInfo},
            .pNext {nullptr},
            .imageView {this->global_descriptors.depth_buffer.getView()},
            .imageLayout {vk::ImageLayout::eDepthAttachmentOptimal},
            .resolveMode {vk::ResolveModeFlagBits::eNone},
            .resolveImageView {nullptr},
            .resolveImageLayout {},
            .loadOp {vk::AttachmentLoadOp::eClear},
            .storeOp {vk::AttachmentStoreOp::eStore},
            .clearValue {
                .depthStencil {
                    vk::ClearDepthStencilValue {.depth {1.0}, .stencil {0}}},
            }}; // namespace game

        const vk::RenderingInfo simpleColorRenderingInfo {
            .sType {vk::StructureType::eRenderingInfo},
            .pNext {nullptr},
            .flags {},
            .renderArea {scissor},
            .layerCount {1},
            .viewMask {0},
            .colorAttachmentCount {1},
            .pColorAttachments {&colorAttachmentInfo},
            .pDepthAttachment {&depthAttachmentInfo},
            .pStencilAttachment {nullptr},
        };

        clearBindings();

        commandBuffer.beginRendering(simpleColorRenderingInfo);

        for (const auto [o, matrixId] : recordablesByPass.at(
                 static_cast<std::size_t>(DynamicRenderingPass::SimpleColor)))
        {
            updateBindings(*o);

            o->record_func(
                commandBuffer,
                **this->game->getRenderer()
                      ->getAllocator()
                      ->lookupPipelineLayout(**o->pipeline),
                matrixId);
        }

        commandBuffer.endRendering();

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::DependencyFlags {},
            {},
            {},
            {vk::ImageMemoryBarrier {
                .sType {vk::StructureType::eImageMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                .dstAccessMask {vk::AccessFlagBits::eColorAttachmentRead},
                .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
                .newLayout {vk::ImageLayout::ePresentSrcKHR},
                .srcQueueFamilyIndex {graphicsQueueIndex},
                .dstQueueFamilyIndex {graphicsQueueIndex},
                .image {thisSwapchainImage},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eColor},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::DependencyFlags {},
            {},
            {},
            {vk::ImageMemoryBarrier {
                .sType {vk::StructureType::eImageMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite},
                .dstAccessMask {
                    vk::AccessFlagBits::eDepthStencilAttachmentRead},
                .oldLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                .newLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                .srcQueueFamilyIndex {graphicsQueueIndex},
                .dstQueueFamilyIndex {graphicsQueueIndex},
                .image {*this->global_descriptors.depth_buffer},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eDepth},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});
    }

    void FrameGenerator::generateFrame(
        Camera newCamera, std::span<const RecordObject> recordObjects)
    {
        const bool resizeOcurred = this->game->getRenderer()->recordOnThread(
            [&](vk::CommandBuffer       commandBuffer,
                u32                     swapchainImageIdx,
                gfx::vulkan::Swapchain& swapchain,
                std::size_t             flyingFrameIdx)
            {
                this->internalGenerateFrame(
                    commandBuffer,
                    swapchainImageIdx,
                    swapchain,
                    flyingFrameIdx,
                    recordObjects);
            });

        this->has_resize_ocurred = resizeOcurred;
        this->camera             = newCamera;

        if (resizeOcurred)
        {
            this->global_descriptors = makeGlobalDescriptors(
                this->game->getRenderer(), this->global_info_descriptor_set);
        }
    }

    vk::DescriptorSet FrameGenerator::getGlobalInfoDescriptorSet() const
    {
        return this->global_info_descriptor_set;
    }

    std::shared_ptr<vk::UniqueDescriptorSetLayout>
    FrameGenerator::getGlobalInfoDescriptorSetLayout() const noexcept
    {
        return this->set_layout;
    }
} // namespace game
