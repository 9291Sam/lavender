#include "frame_generator.hpp"
#include "game.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/vulkan/frame_manager.hpp"
#include "gfx/vulkan/image.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <bit>
#include <compare>
#include <cstddef>
#include <gfx/renderer.hpp>
#include <gfx/vulkan/instance.hpp>
#include <gfx/vulkan/swapchain.hpp>
#include <gfx/window.hpp>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>
#include <random>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
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
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageAspectFlagBits::eDepth,
            vk::ImageTiling::eOptimal,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            "Global Depth Buffer"};

        gfx::vulkan::WriteOnlyBuffer<glm::mat4> mvpMatrices {
            renderer->getAllocator(),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
            1024,
            "Global MVP Matrices"};

        gfx::vulkan::WriteOnlyBuffer<GlobalFrameInfo> globalFrameInfo {
            renderer->getAllocator(),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
            1,
            "Global Frame Info"};

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
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
            vk::ImageAspectFlagBits::eColor,
            vk::ImageTiling::eOptimal,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            "Visible Voxel Image"};

        vk::UniqueSampler doNothingSampler =
            renderer->getDevice()->getDevice().createSamplerUnique(vk::SamplerCreateInfo {
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

        if constexpr (util::isDebugBuild())
        {
            renderer->getDevice()->getDevice().setDebugUtilsObjectNameEXT(
                vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eSampler},
                    .objectHandle {std::bit_cast<u64>(*doNothingSampler)},
                    .pObjectName {"Do Nothing Sampler"},
                });
        }

        const vk::DescriptorImageInfo depthBufferInfo {
            .sampler {nullptr},
            .imageView {depthBuffer.getView()},
            .imageLayout {vk::ImageLayout::eDepthReadOnlyOptimal},
        };
        const vk::DescriptorImageInfo visibleVoxelImageInfo {
            .sampler {nullptr},
            .imageView {visibleVoxelImage.getView()},
            .imageLayout {vk::ImageLayout::eGeneral},
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
                    .descriptorType {vk::DescriptorType::eStorageImage},
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
            .do_nothing_sampler {std::move(doNothingSampler)},
        };
    }

    std::strong_ordering FrameGenerator::RecordObject::RecordObject::operator<=> (
        const RecordObject& other) const noexcept
    {
        if (util::toUnderlying(this->render_pass) != util::toUnderlying(other.render_pass))
        {
            return util::toUnderlying(this->render_pass) <=> util::toUnderlying(other.render_pass);
        }

        if (this->pipeline != other.pipeline)
        {
            return this->pipeline <=> other.pipeline;
        }

        if (this->pipeline && other.pipeline && this->pipeline->get() != other.pipeline->get())
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
        , set_layout {this->game->getRenderer()->getAllocator()->cacheDescriptorSetLayout(
              gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {
                  .bindings {
                      vk::DescriptorSetLayoutBinding {
                          .binding {0},
                          .descriptorType {vk::DescriptorType::eUniformBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {1},
                          .descriptorType {vk::DescriptorType::eUniformBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {2},
                          .descriptorType {vk::DescriptorType::eSampledImage},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {3},
                          .descriptorType {vk::DescriptorType::eStorageImage},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {4},
                          .descriptorType {vk::DescriptorType::eSampler},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                  },
                  .name {"Global Descriptor Set Layout"}})}
        , global_info_descriptor_set {this->game->getRenderer()
                                          ->getAllocator()
                                          ->allocateDescriptorSet(
                                              **this->set_layout, "Global Descriptor Set")}
        , global_descriptors {
              makeGlobalDescriptors(this->game->getRenderer(), this->global_info_descriptor_set)}
    {
        ImGui::CreateContext();

        const vk::DynamicLoader& loader = this->game->getRenderer()->getInstance()->getLoader();

        // whatever the fuck this is
        // Absolute nastiness, thanks `@karnage`!
        auto getFn = [&loader](const char* name)
        {
            return loader.getProcAddress<PFN_vkVoidFunction>(name);
        };
        auto lambda = +[](const char* name, void* ud)
        {
            const auto* gf = reinterpret_cast<decltype(getFn)*>(ud); // NOLINT

            void (*ptr)() = nullptr;

            ptr = (*gf)(name);

            // HACK: sometimes function ending in KHR are removed if they exist in core,
            // pave over this
            if (std::string_view nameView {name}; ptr == nullptr && nameView.ends_with("KHR"))
            {
                nameView.remove_suffix(3);

                std::string apiString {nameView};

                ptr = (*gf)(apiString.c_str());
            }

            util::assertFatal(ptr != nullptr, "Failed to load {}", name);

            return ptr;
        };
        ImGui_ImplVulkan_LoadFunctions(lambda, &getFn);

        this->game->getRenderer()->getWindow()->initializeImgui();

        this->game->getRenderer()->getDevice()->acquireQueue(
            gfx::vulkan::Device::QueueType::Graphics,
            [&](vk::Queue q)
            {
                const VkFormat colorFormat =
                    static_cast<VkFormat>(gfx::Renderer::ColorFormat.format);

                const VkFormat depthFormat = static_cast<VkFormat>(gfx::Renderer::DepthFormat);

                ImGui_ImplVulkan_InitInfo initInfo {
                    .Instance {**this->game->getRenderer()->getInstance()},
                    .PhysicalDevice {this->game->getRenderer()->getDevice()->getPhysicalDevice()},
                    .Device {this->game->getRenderer()->getDevice()->getDevice()},
                    .Queue {std::bit_cast<VkQueue>(q)},
                    .DescriptorPool {this->game->getRenderer()->getAllocator()->getRawPool()},
                    .RenderPass {nullptr},
                    .MinImageCount {gfx::vulkan::FramesInFlight},
                    .ImageCount {gfx::vulkan::FramesInFlight},
                    .MSAASamples {VK_SAMPLE_COUNT_1_BIT},
                    .PipelineCache {this->game->getRenderer()->getAllocator()->getRawCache()},
                    .Subpass {0},
                    .UseDynamicRendering {true},
                    .PipelineRenderingCreateInfo {
                        .sType {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO},
                        .pNext {nullptr},
                        .viewMask {0},
                        .colorAttachmentCount {1},
                        .pColorAttachmentFormats {&colorFormat},
                        .depthAttachmentFormat {depthFormat},
                        .stencilAttachmentFormat {},
                    },
                    .Allocator {nullptr},
                    .CheckVkResultFn {[](VkResult err)
                                      {
                                          util::assertWarn(
                                              err == VK_SUCCESS,
                                              "Imgui ResultCheckFailed {}",
                                              vk::to_string(vk::Result {err}));
                                      }},
                    .MinAllocationSize {1024UZ * 1024}};

                ImGui_ImplVulkan_Init(&initInfo);

                ImGuiIO& io = ImGui::GetIO();

                static ImWchar unifont_ranges[] = {0x0001, 0xFFFF, 0};   // Full range for Unifont
                static ImWchar emoji_ranges[]   = {0x1F300, 0x1FAFF, 0}; // Emoji range for OpenMoji

                static ImWchar merged_range[] = {0x0001, 0xFFFFF, 0};

                // static ImWchar ranges[] = {0x0001, 0x1FFFF, 0};
                // // static ImFontConfig fontConfig;
                // // fontConfig.OversampleH = fontConfig.OversampleV = 1;
                // // fontConfig.MergeMode                            = true;
                // // fontConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

                // static ImWchar ranges2[] = {
                //     0x0001,
                //     0x0FAF, // Basic Latin + Latin Supplement
                //     0};

                // this->font = io.Fonts->AddFontFromFileTTF(
                //     "../src/OpenMoji-color-sbix.ttf", 16, nullptr, ranges);
                // this->font =
                //     io.Fonts->AddFontFromFileTTF("../src/unifont-15.1.05.otf", 16, nullptr,
                //     ranges);

                // Load Unifont with full Unicode range
                ImFontConfig fontConfigUnifont;
                fontConfigUnifont.OversampleH = fontConfigUnifont.OversampleV = 1;
                fontConfigUnifont.MergeMode                                   = false;
                fontConfigUnifont.SizePixels                                  = 12;
                fontConfigUnifont.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
                this->font = io.Fonts->AddFontFromFileTTF(
                    "../src/unifont-16.0.01.otf", 16, &fontConfigUnifont, merged_range);

                // Load OpenMoji with emoji range and merge into Unifont
                // ImFontConfig fontConfigEmoji;
                // fontConfigEmoji.OversampleH = fontConfigEmoji.OversampleV = 1;
                // fontConfigEmoji.MergeMode = true; // Merge into the existing font
                // fontConfigEmoji.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
                // this->font = io.Fonts->AddFontFromFileTTF(
                //     "../src/noto-untouchedsvg.ttf", 16, &fontConfigEmoji, emoji_ranges);

                io.Fonts->Build();

                ImGui_ImplVulkan_CreateFontsTexture();
            });
    }

    FrameGenerator::~FrameGenerator()
    {
        ImGui_ImplVulkan_Shutdown();
    }

    void FrameGenerator::internalGenerateFrame(
        vk::CommandBuffer             commandBuffer,
        u32                           swapchainImageIdx,
        const gfx::vulkan::Swapchain& swapchain,
        std::size_t,
        std::span<const FrameGenerator::RecordObject> recordObjects)
    {
        std::array<
            std::vector<std::pair<const FrameGenerator::RecordObject*, u32>>,
            static_cast<std::size_t>(
                FrameGenerator::DynamicRenderingPass::DynamicRenderingPassMaxValue)>
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
                localMvpMatrices.push_back(
                    this->camera.getPerspectiveMatrix(*this->game, *o.transform));

                thisMatrixId = nextFreeMvpMatrix;

                nextFreeMvpMatrix += 1;

                util::assertFatal(thisMatrixId < 1024, "too many matriices");
            }

            recordablesByPass.at(static_cast<std::size_t>(util::toUnderlying(o.render_pass)))
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

        for (std::vector<std::pair<const FrameGenerator::RecordObject*, u32>>& recordVec :
             recordablesByPass)
        {
            std::sort(
                recordVec.begin(),
                recordVec.end(),
                [](const auto& l, const auto& r)
                {
                    return *l.first < *r.first;
                });
        }

        const vk::Extent2D renderExtent = swapchain.getExtent();

        const vk::Rect2D scissor {.offset {vk::Offset2D {.x {0}, .y {0}}}, .extent {renderExtent}};

        const vk::Viewport renderViewport {
            .x {0.0},
            .y {static_cast<F32>(renderExtent.height)},
            .width {static_cast<F32>(renderExtent.width)},
            .height {static_cast<F32>(renderExtent.height) * -1.0f},
            .minDepth {0.0},
            .maxDepth {1.0},
        };

        const gfx::vulkan::Device& device = *this->game->getRenderer()->getDevice();

        const vk::Image     thisSwapchainImage     = swapchain.getImages()[swapchainImageIdx];
        const vk::ImageView thisSwapchainImageView = swapchain.getViews()[swapchainImageIdx];
        const u32           graphicsQueueIndex =
            device // NOLINT
                .getFamilyOfQueueType(gfx::vulkan::Device::QueueType::Graphics)
                .value();

        vk::Pipeline                     currentlyBoundPipeline = nullptr;
        std::array<vk::DescriptorSet, 4> currentlyBoundDescriptors {
            nullptr, nullptr, nullptr, nullptr};

        auto updateBindings = [&](const RecordObject& o)
        {
            util::assertFatal(o.pipeline != nullptr, "nullshared");
            util::assertFatal(**o.pipeline != nullptr, "nullpipeline");

            if (currentlyBoundPipeline != **o.pipeline)
            {
                commandBuffer.bindPipeline(
                    this->game->getRenderer()->getAllocator()->lookupPipelineBindPoint(
                        **o.pipeline),
                    **o.pipeline);
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
                        this->game->getRenderer()->getAllocator()->lookupPipelineBindPoint(
                            **o.pipeline),
                        **this->game->getRenderer()->getAllocator()->lookupPipelineLayout(
                            **o.pipeline),
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

        auto doRenderPass = [&](DynamicRenderingPass                   p,
                                vk::RenderingInfo                      info,
                                std::function<void(vk::CommandBuffer)> extraCommands = {})
        {
            clearBindings();

            commandBuffer.beginRendering(info);

            for (const auto [o, matrixId] : recordablesByPass.at(static_cast<std::size_t>(p)))
            {
                updateBindings(*o);

                o->record_func(
                    commandBuffer,
                    **this->game->getRenderer()->getAllocator()->lookupPipelineLayout(
                        **o->pipeline),
                    matrixId);
            }

            if (extraCommands != nullptr)
            {
                extraCommands(commandBuffer);
            }

            commandBuffer.endRendering();
        };

        auto doComputePass = [&](DynamicRenderingPass p)
        {
            clearBindings();

            for (const auto [o, matrixId] : recordablesByPass[static_cast<std::size_t>(p)])
            {
                updateBindings(*o);

                o->record_func(commandBuffer, nullptr, 0);
            }
        };

        auto doTransferPass = [&](DynamicRenderingPass p)
        {
            for (const auto [o, matrixId] : recordablesByPass[static_cast<std::size_t>(p)])
            {
                o->record_func(commandBuffer, nullptr, 0);
            }
        };

        doTransferPass(DynamicRenderingPass::PreFrameUpdate);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::DependencyFlags {},
            {vk::MemoryBarrier {
                .sType {vk::StructureType::eMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eTransferWrite},
                .dstAccessMask {vk::AccessFlagBits::eMemoryRead},
            }},
            {},
            {});

        if (this->has_resize_ocurred)
        {
            const std::span<const vk::Image> swapchainImages = swapchain.getImages();

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
                    .dstAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentRead},
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

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eFragmentShader,
                {},
                {},
                {},
                {vk::ImageMemoryBarrier {
                    .sType {vk::StructureType::eImageMemoryBarrier},
                    .pNext {nullptr},
                    .srcAccessMask {vk::AccessFlagBits::eNone},
                    .dstAccessMask {vk::AccessFlagBits::eShaderRead},
                    .oldLayout {vk::ImageLayout::eUndefined},
                    .newLayout {vk::ImageLayout::eGeneral},
                    .srcQueueFamilyIndex {graphicsQueueIndex},
                    .dstQueueFamilyIndex {graphicsQueueIndex},
                    .image {*this->global_descriptors.visible_voxel_image},
                    .subresourceRange {vk::ImageSubresourceRange {
                        .aspectMask {vk::ImageAspectFlagBits::eColor},
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
                .srcAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentRead},
                .dstAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentWrite},
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

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::DependencyFlags {},
            {},
            {},
            {vk::ImageMemoryBarrier {
                .sType {vk::StructureType::eImageMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eShaderRead},
                .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                .oldLayout {vk::ImageLayout::eGeneral},
                .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
                .srcQueueFamilyIndex {graphicsQueueIndex},
                .dstQueueFamilyIndex {graphicsQueueIndex},
                .image {*this->global_descriptors.visible_voxel_image},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eColor},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});

        commandBuffer.setViewport(0, {renderViewport});
        commandBuffer.setScissor(0, {scissor});

        // Voxel Render
        {
            const vk::RenderingAttachmentInfo colorAttachmentInfo {
                .sType {vk::StructureType::eRenderingAttachmentInfo},
                .pNext {nullptr},
                .imageView {this->global_descriptors.visible_voxel_image.getView()},
                .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
                .resolveMode {vk::ResolveModeFlagBits::eNone},
                .resolveImageView {nullptr},
                .resolveImageLayout {},
                .loadOp {vk::AttachmentLoadOp::eClear},
                .storeOp {vk::AttachmentStoreOp::eStore},
                .clearValue {
                    vk::ClearColorValue {.uint32 {{~u32 {0}, ~u32 {0}, ~u32 {0}, ~u32 {0}}}},
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
                    .depthStencil {vk::ClearDepthStencilValue {.depth {1.0}, .stencil {0}}},
                }};

            const vk::RenderingInfo voxelRenderPassInfo {
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

            doRenderPass(DynamicRenderingPass::VoxelRenderer, voxelRenderPassInfo);
        }

        // barrier on visible voxel image to make it readable
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags {},
            {},
            {},
            {vk::ImageMemoryBarrier {
                .sType {vk::StructureType::eImageMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                .dstAccessMask {vk::AccessFlagBits::eShaderRead},
                .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
                .newLayout {vk::ImageLayout::eGeneral},
                .srcQueueFamilyIndex {graphicsQueueIndex},
                .dstQueueFamilyIndex {graphicsQueueIndex},
                .image {*this->global_descriptors.visible_voxel_image},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eColor},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            }});

        // Visibility Detection
        {
            doComputePass(DynamicRenderingPass::VoxelVisibilityDetection);
        }

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::DependencyFlags {},
            {vk::MemoryBarrier {
                .sType {vk::StructureType::eMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eShaderWrite},
                .dstAccessMask {vk::AccessFlagBits::eMemoryRead},
            }},
            {},
            {});

        // Color Calculation
        {
            doComputePass(DynamicRenderingPass::VoxelColorCalculation);
        }

        // barrier on storage writes before transfer
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags {},
            {vk::MemoryBarrier {
                .sType {vk::StructureType::eMemoryBarrier},
                .pNext {nullptr},
                .srcAccessMask {vk::AccessFlagBits::eShaderWrite},
                .dstAccessMask {vk::AccessFlagBits::eShaderRead},
            }},
            {},
            {});

        // VoxelColorTransfer
        {
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

            const vk::RenderingInfo voxelColorTransferRenderingInfo {
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

            // clear swapchain image
            doRenderPass(DynamicRenderingPass::VoxelColorTransfer, voxelColorTransferRenderingInfo);
        }

        // Simple Color
        {
            const vk::RenderingAttachmentInfo colorAttachmentInfo {
                .sType {vk::StructureType::eRenderingAttachmentInfo},
                .pNext {nullptr},
                .imageView {thisSwapchainImageView},
                .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
                .resolveMode {vk::ResolveModeFlagBits::eNone},
                .resolveImageView {nullptr},
                .resolveImageLayout {},
                .loadOp {vk::AttachmentLoadOp::eLoad},
                .storeOp {vk::AttachmentStoreOp::eStore},
                .clearValue {},
            };

            const vk::RenderingAttachmentInfo depthAttachmentInfo {
                .sType {vk::StructureType::eRenderingAttachmentInfo},
                .pNext {nullptr},
                .imageView {this->global_descriptors.depth_buffer.getView()},
                .imageLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                .resolveMode {vk::ResolveModeFlagBits::eNone},
                .resolveImageView {nullptr},
                .resolveImageLayout {},
                .loadOp {vk::AttachmentLoadOp::eLoad},
                .storeOp {vk::AttachmentStoreOp::eStore},
                .clearValue {}};

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

            doRenderPass(
                DynamicRenderingPass::SimpleColor,
                simpleColorRenderingInfo,
                [&](vk::CommandBuffer commandBuffer)
                {
                    // imgui new frame
                    ImGui_ImplVulkan_NewFrame();
                    ImGui_ImplGlfw_NewFrame();
                    ImGui::NewFrame();

                    const ImGuiViewport* const viewport = ImGui::GetMainViewport();

                    const auto [x, y] = viewport->Size;

                    const ImVec2 desiredConsoleSize {2 * x / 9, y}; // 2 / 9 is normal

                    ImGui::SetNextWindowPos(
                        ImVec2 {std::ceil(viewport->Size.x - desiredConsoleSize.x), 0});
                    ImGui::SetNextWindowSize(desiredConsoleSize);

                    if (ImGui::Begin(
                            "Console",
                            nullptr,
                            ImGuiWindowFlags_NoResize |            // NOLINT
                                ImGuiWindowFlags_NoSavedSettings | // NOLINT
                                ImGuiWindowFlags_NoMove |          // NOLINT
                                ImGuiWindowFlags_NoDecoration))    // NOLINT
                    {
                        static constexpr float WindowPadding = 5.0f;

                        util::assertFatal(this->font != nullptr, "oopers");

                        ImGui::PushFont(this->font);
                        ImGui::PushStyleVar(
                            ImGuiStyleVar_WindowPadding, ImVec2(WindowPadding, WindowPadding));
                        {
                            if (ImGui::Button("Button"))
                            {
                                util::logTrace("pressed button");
                            }

                            const std::string playerPosition = std::format(
                                "Player position: {}", glm::to_string(camera.getPosition()));
                            ImGui::TextWrapped("%s", playerPosition.c_str());

                            const std::string fpsAndTps = std::format(
                                "FPS: {:.3f} | Frame Time (ms): {:.3f}",
                                1.0 / this->game->getRenderer()->getWindow()->getDeltaTimeSeconds(),
                                this->game->getRenderer()->getWindow()->getDeltaTimeSeconds());

                            ImGui::TextWrapped("%s", (const char*)u8"こん✨ちは");
                        }

                        ImGui::PopStyleVar();
                        ImGui::PopFont();

                        ImGui::End();
                    }
                    // ImGui::ShowDemoWindow();

                    ImGui::Render();

                    ImGui_ImplVulkan_RenderDrawData(
                        ImGui::GetDrawData(), static_cast<VkCommandBuffer>(commandBuffer));
                });
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
                .srcAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentWrite},
                .dstAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentRead},
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

    void
    FrameGenerator::generateFrame(Camera newCamera, std::span<const RecordObject> recordObjects)
    {
        const bool resizeOcurred = this->game->getRenderer()->recordOnThread(
            [&](vk::CommandBuffer       commandBuffer,
                u32                     swapchainImageIdx,
                gfx::vulkan::Swapchain& swapchain,
                std::size_t             flyingFrameIdx)
            {
                this->internalGenerateFrame(
                    commandBuffer, swapchainImageIdx, swapchain, flyingFrameIdx, recordObjects);
            });

        this->has_resize_ocurred = resizeOcurred;
        this->camera             = newCamera;

        if (resizeOcurred)
        {
            this->global_descriptors =
                makeGlobalDescriptors(this->game->getRenderer(), this->global_info_descriptor_set);
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
