#include "frame_generator.hpp"
#include "gfx/vulkan/device.hpp"
#include "util/misc.hpp"
#include <compare>
#include <cstddef>
#include <gfx/vulkan/swapchain.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game::frame
{
    std::strong_ordering RecordObject::RecordObject::operator<=> (
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

    void generateFrame(
        vk::CommandBuffer             commandBuffer,
        U32                           swapchainImageIdx,
        const gfx::vulkan::Swapchain& swapchain,
        const gfx::vulkan::Device&    device,
        std::span<RecordObject>       recordObjects)
    {
        std::array<
            std::vector<RecordObject>,
            static_cast<std::size_t>(
                DynamicRenderingPass::DynamicRenderingPassMaxValue)>
            recordablesByPass;

        for (RecordObject& o : recordObjects)
        {
            recordablesByPass
                .at(static_cast<std::size_t>(util::toUnderlying(o.render_pass)))
                .push_back(std::move(o));
        }

        for (std::vector<RecordObject>& recordVec : recordablesByPass)
        {
            std::sort(recordVec.begin(), recordVec.end());
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

        const vk::Image thisSwapchainImage =
            swapchain.getImages()[swapchainImageIdx];
        const vk::ImageView thisSwapchainImageView =
            swapchain.getViews()[swapchainImageIdx];
        const U32 graphicsQueueIndex =
            device
                .getFamilyOfQueueType(gfx::vulkan::Device::QueueType::Graphics)
                .value();

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

        // renderer.device.dynamic_rendering_fns.cmd_begin_rendering(
        //     command_buffer, &vk::RenderingInfo {
        //         s_type: vk::StructureType::RENDERING_INFO,
        //         p_next: std::ptr::null(),
        //         flags: vk::RenderingFlags::empty(),
        //         render_area: render_rect,
        //         layer_count: 1,
        //         view_mask: 0,
        //         color_attachment_count: 1,
        //         p_color_attachments: &vk::RenderingAttachmentInfo {
        //             s_type: vk::StructureType::RENDERING_ATTACHMENT_INFO,
        //             p_next: std::ptr::null(),
        //             image_view:
        //                 swapchain.get_views()[swapchain_image_idx as usize],
        //             image_layout: vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
        //             resolve_mode: vk::ResolveModeFlags::NONE,
        //             resolve_image_view: vk::ImageView::null(),
        //             resolve_image_layout: vk::ImageLayout::UNDEFINED,
        //             load_op: vk::AttachmentLoadOp::CLEAR,
        //             store_op: vk::AttachmentStoreOp::STORE,
        //             clear_value: vk::ClearValue {
        //                 color: vk::
        //                 ClearColorValue {float32: [0.1, 0.4, 0.3, 0.0]}
        //             },
        //             _marker: std::marker::PhantomData
        //         },
        //         p_depth_attachment: &vk::RenderingAttachmentInfo {
        //             s_type: vk::StructureType::RENDERING_ATTACHMENT_INFO,
        //             p_next: std::ptr::null(),
        //             image_view: depth_buffer.get_image_view(),
        //             image_layout: vk::ImageLayout::DEPTH_ATTACHMENT_OPTIMAL,
        //             resolve_mode: vk::ResolveModeFlags::NONE,
        //             resolve_image_view: vk::ImageView::null(),
        //             resolve_image_layout: vk::ImageLayout::UNDEFINED,
        //             load_op: vk::AttachmentLoadOp::CLEAR,
        //             store_op: vk::AttachmentStoreOp::STORE,
        //             clear_value: vk::ClearValue {
        //                 depth_stencil: vk::
        //                 ClearDepthStencilValue {depth: 1.0, stencil: 0}
        //             },
        //             _marker: std::marker::PhantomData
        //         },
        //         p_stencil_attachment: std::ptr::null(),
        //         _marker: std::marker::PhantomData
        //     });

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

        // const vk::RenderingAttachmentInfo depthAttachmentInfo {
        //     .sType {vk::StructureType::eRenderingAttachmentInfo},
        //     .pNext {nullptr},
        //     .imageView {thisSwapchainImageView},
        //     .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
        //     .resolveMode {vk::ResolveModeFlagBits::eNone},
        //     .resolveImageView {nullptr},
        //     .resolveImageLayout {},
        //     .loadOp {vk::AttachmentLoadOp::eClear},
        //     .storeOp {vk::AttachmentStoreOp::eStore},
        //     .clearValue {
        //         vk::ClearColorValue {.float32 {{0.709f, 0.494f,
        //         0.862f, 1.0f}}},
        //     },
        // };

        const vk::RenderingInfo simpleColorRenderingInfo {
            .sType {vk::StructureType::eRenderingInfo},
            .pNext {nullptr},
            .flags {},
            .renderArea {scissor},
            .layerCount {1},
            .viewMask {0},
            .colorAttachmentCount {1},
            .pColorAttachments {&colorAttachmentInfo},
            .pDepthAttachment {nullptr},
            .pStencilAttachment {nullptr},
        };

        commandBuffer.beginRendering(simpleColorRenderingInfo);

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
    }

} // namespace game::frame
