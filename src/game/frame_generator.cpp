#include "frame_generator.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/vulkan/image.hpp"
#include "util/misc.hpp"
#include <compare>
#include <cstddef>
#include <gfx/renderer.hpp>
#include <gfx/vulkan/swapchain.hpp>
#include <gfx/window.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game
{
    gfx::vulkan::Image2D makeDepthBuffer(
        const gfx::vulkan::Allocator* allocator,
        vk::Device                    device,
        vk::Extent2D                  extent)
    {
        return gfx::vulkan::Image2D {
            allocator,
            device,
            extent,
            vk::Format::eD32Sfloat,
            vk::ImageLayout::eUndefined,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageAspectFlagBits::eDepth,
            vk::ImageTiling::eOptimal,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            "Depth buffer"};
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

    FrameGenerator::FrameGenerator(const gfx::Renderer* renderer_)
        : renderer {renderer_}
        , needs_resize_transitions {true}
        , depth_buffer {makeDepthBuffer(
              this->renderer->getAllocator(),
              this->renderer->getDevice()->getDevice(),
              this->renderer->getWindow()->getFramebufferSize())}
    {}

    void FrameGenerator::internalGenerateFrame(
        vk::CommandBuffer                             commandBuffer,
        U32                                           swapchainImageIdx,
        const gfx::vulkan::Swapchain&                 swapchain,
        std::span<const FrameGenerator::RecordObject> recordObjects)
    {
        std::array<
            std::vector<const FrameGenerator::RecordObject*>,
            static_cast<std::size_t>(FrameGenerator::DynamicRenderingPass::
                                         DynamicRenderingPassMaxValue)>
            recordablesByPass;

        for (const FrameGenerator::RecordObject& o : recordObjects)
        {
            util::assertFatal(o.pipeline != nullptr, "Nullpipeline");

            recordablesByPass
                .at(static_cast<std::size_t>(util::toUnderlying(o.render_pass)))
                .push_back(&o);
        }

        for (std::vector<const FrameGenerator::RecordObject*>& recordVec :
             recordablesByPass)
        {
            std::sort(
                recordVec.begin(),
                recordVec.end(),
                [](auto* l, auto* r)
                {
                    return *l < *r;
                });
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

        const gfx::vulkan::Device& device = *this->renderer->getDevice();

        const vk::Image thisSwapchainImage =
            swapchain.getImages()[swapchainImageIdx];
        const vk::ImageView thisSwapchainImageView =
            swapchain.getViews()[swapchainImageIdx];
        const U32 graphicsQueueIndex =
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

            for (U32 i = 0; i < 4; ++i)
            {
                if (currentlyBoundDescriptors[i] != o.descriptors[i]) // NOLINT
                {
                    currentlyBoundDescriptors[i] = o.descriptors[i]; // NOLINT

                    for (U32 j = i + 1; j < 4; ++j)
                    {
                        currentlyBoundDescriptors[i] = nullptr; // NOLINT
                    }

                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        **this->renderer->getAllocator()->lookupPipelineLayout(
                            **o.pipeline),
                        i,
                        {o.descriptors[i]}, // NOLINT
                        {0});
                }
            }
        };

        auto clearBindings = [&]
        {
            currentlyBoundPipeline    = nullptr;
            currentlyBoundDescriptors = {nullptr, nullptr, nullptr, nullptr};
        };

        if (this->needs_resize_transitions)
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
                    .image {*this->depth_buffer},
                    .subresourceRange {vk::ImageSubresourceRange {
                        .aspectMask {vk::ImageAspectFlagBits::eDepth},
                        .baseMipLevel {0},
                        .levelCount {1},
                        .baseArrayLayer {0},
                        .layerCount {1}}},
                }});
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
                .image {*this->depth_buffer},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eDepth},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});

        commandBuffer.setViewport(0, {renderViewport});
        commandBuffer.setScissor(0, {scissor});

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
            .imageView {this->depth_buffer.getView()},
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

        for (const RecordObject* o : recordablesByPass.at(
                 static_cast<std::size_t>(DynamicRenderingPass::SimpleColor)))
        {
            updateBindings(*o);

            o->record_func(commandBuffer);
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
                .image {*this->depth_buffer},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eDepth},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});
    }

    void FrameGenerator::generateFrame(std::span<RecordObject> recordObjects)
    {
        const bool resizeOcurred = this->renderer->recordOnThread(
            [&](vk::CommandBuffer       commandBuffer,
                U32                     swapchainImageIdx,
                gfx::vulkan::Swapchain& swapchain)
            {
                this->internalGenerateFrame(
                    commandBuffer, swapchainImageIdx, swapchain, recordObjects);
            });

        this->needs_resize_transitions = resizeOcurred;

        if (resizeOcurred)
        {
            this->depth_buffer = makeDepthBuffer(
                this->renderer->getAllocator(),
                this->renderer->getDevice()->getDevice(),
                this->renderer->getWindow()->getFramebufferSize());
        }
    }
} // namespace game
