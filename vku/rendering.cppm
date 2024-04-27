module;

#include <cassert>

export module vku:rendering;

import std;
export import vulkan_hpp;
export import vk_mem_alloc_hpp;
import :details;
import :image;
import :utils;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace vku {
    export struct Attachment {
        Image image;
        vk::raii::ImageView view;
    };

    class AttachmentGroupBase {
    public:
        vk::Extent2D extent;

        explicit AttachmentGroupBase(
            const vk::Extent2D &extent
        ) : extent { extent } { }
        AttachmentGroupBase(const AttachmentGroupBase&) = delete;
        AttachmentGroupBase(AttachmentGroupBase&&) noexcept = default;
        auto operator=(const AttachmentGroupBase&) -> AttachmentGroupBase& = delete;
        auto operator=(AttachmentGroupBase&&) noexcept -> AttachmentGroupBase& = default;
        virtual ~AttachmentGroupBase() = default;

        [[nodiscard]] auto storeImage(
            std::unique_ptr<AllocatedImage> image
        ) -> const AllocatedImage& {
            return *storedImage.emplace_back(std::move(image));
        }

        [[nodiscard]] auto storeImage(
            std::derived_from<AllocatedImage> auto &&image
        ) -> const AllocatedImage& {
            return *storedImage.emplace_back(std::make_unique<std::remove_cvref_t<decltype(image)>>(FWD(image)));
        }

        auto setViewport(
            vk::CommandBuffer commandBuffer,
            bool negativeViewport = false
        ) const -> void {
            commandBuffer.setViewport(0, negativeViewport
                ? vk::Viewport {
                    0, static_cast<float>(extent.height),
                    static_cast<float>(extent.width), -static_cast<float>(extent.height),
                    0.f, 1.f,
                }
                : vk::Viewport {
                    0, 0,
                    static_cast<float>(extent.width), static_cast<float>(extent.height),
                    0.f, 1.f,
                }
            );
        }

        auto setScissor(
            vk::CommandBuffer commandBuffer
        ) const -> void {
            commandBuffer.setScissor(0, vk::Rect2D {
                { 0, 0 },
                extent,
            });
        }

        [[nodiscard]] virtual auto getRenderingInfo(
            std::span<const std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearColorValue>> colorAttachmentInfos = {},
            std::optional<std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearDepthStencilValue>> depthStencilAttachmentInfo = {}
        ) const -> RefHolder<vk::RenderingInfo, std::vector<vk::RenderingAttachmentInfo>, std::optional<vk::RenderingAttachmentInfo>> = 0;

    protected:
        std::vector<std::unique_ptr<AllocatedImage>> storedImage;

        [[nodiscard]] auto createAttachmentImage(
            vma::Allocator allocator,
            vk::Format format,
            vk::SampleCountFlagBits sampleCount,
            vk::ImageUsageFlags usage,
            const vma::AllocationCreateInfo &allocationCreateInfo
        ) const -> AllocatedImage {
            return { allocator, vk::ImageCreateInfo {
                {},
                vk::ImageType::e2D,
                format,
                vk::Extent3D { extent, 1 },
                1, 1,
                sampleCount,
                vk::ImageTiling::eOptimal,
                usage,
            }, allocationCreateInfo };
        }
    };

    export struct AttachmentGroup : AttachmentGroupBase {
        std::vector<Attachment> colorAttachments;
        std::optional<Attachment> depthStencilAttachment;

        explicit AttachmentGroup(
            const vk::Extent2D &extent
        ) : AttachmentGroupBase { extent } { }
        AttachmentGroup(const AttachmentGroup&) = delete;
        AttachmentGroup(AttachmentGroup&&) noexcept = default;
        auto operator=(const AttachmentGroup&) -> AttachmentGroup& = delete;
        auto operator=(AttachmentGroup&&) noexcept -> AttachmentGroup& = default;
        ~AttachmentGroup() override = default;

        auto addColorAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return colorAttachments.emplace_back(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        [[nodiscard]] auto createColorImage(
            vma::Allocator allocator,
            vk::Format format,
            vk::ImageUsageFlags usage = {},
            const vma::AllocationCreateInfo &allocationCreateInfo = { {}, vma::MemoryUsage::eAutoPreferDevice }
        ) const -> AllocatedImage {
            return createAttachmentImage(allocator, format, vk::SampleCountFlagBits::e1, usage | vk::ImageUsageFlagBits::eColorAttachment, allocationCreateInfo);
        }

        auto setDepthAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return depthStencilAttachment.emplace(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        auto setStencilAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return depthStencilAttachment.emplace(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        auto setDepthStencilAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return depthStencilAttachment.emplace(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        [[nodiscard]] auto createDepthStencilImage(
            vma::Allocator allocator,
            vk::Format format,
            vk::ImageUsageFlags usage = {},
            const vma::AllocationCreateInfo &allocationCreateInfo = { {}, vma::MemoryUsage::eAutoPreferDevice }
        ) const -> AllocatedImage {
            return createAttachmentImage(allocator, format, vk::SampleCountFlagBits::e1, usage | vk::ImageUsageFlagBits::eDepthStencilAttachment, allocationCreateInfo);
        }

        auto getRenderingInfo(
            std::span<const std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearColorValue>> colorAttachmentInfos = {},
            std::optional<std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearDepthStencilValue>> depthStencilAttachmentInfo = {}
        ) const -> RefHolder<vk::RenderingInfo, std::vector<vk::RenderingAttachmentInfo>, std::optional<vk::RenderingAttachmentInfo>> override {
            assert(colorAttachments.size() == colorAttachmentInfos.size() && "Color attachment info count mismatch");
            assert(depthStencilAttachment.has_value() == depthStencilAttachmentInfo.has_value() && "Depth-stencil attachment info mismatch");
            return {
                [this](std::span<const vk::RenderingAttachmentInfo> colorAttachmentInfos, const std::optional<vk::RenderingAttachmentInfo> &depthStencilAttachmentInfo) {
                    return vk::RenderingInfo {
                        {},
                        { { 0, 0 }, extent },
                        1,
                        {},
                        colorAttachmentInfos,
                        depthStencilAttachmentInfo ? &*depthStencilAttachmentInfo : nullptr,
                    };
                },
                std::views::zip_transform([](const Attachment &attachment, const std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearColorValue> &info) {
                    return vk::RenderingAttachmentInfo {
                        *attachment.view, vk::ImageLayout::eColorAttachmentOptimal,
                        {}, {}, {},
                        std::get<0>(info), std::get<1>(info), std::get<2>(info),
                    };
                }, colorAttachments, colorAttachmentInfos) | std::ranges::to<std::vector>(),
                depthStencilAttachment.transform([info = *depthStencilAttachmentInfo](const Attachment &attachment) {
                    return vk::RenderingAttachmentInfo {
                        *attachment.view, vk::ImageLayout::eDepthStencilAttachmentOptimal,
                        {}, {}, {},
                        std::get<0>(info), std::get<1>(info), std::get<2>(info),
                    };
                })
            };
        }
    };

    export struct MsaaAttachmentGroup : AttachmentGroupBase {
        struct MsaaAttachment : Attachment {
            Image resolveImage;
            vk::raii::ImageView resolveView;
        };

        vk::SampleCountFlagBits sampleCount;
        std::vector<MsaaAttachment> colorAttachments;
        std::optional<Attachment> depthStencilAttachment;

        explicit MsaaAttachmentGroup(
            const vk::Extent2D &extent,
            vk::SampleCountFlagBits sampleCount
        ) : AttachmentGroupBase { extent },
            sampleCount { sampleCount } { }
        MsaaAttachmentGroup(const MsaaAttachmentGroup&) = delete;
        MsaaAttachmentGroup(MsaaAttachmentGroup&&) noexcept = default;
        auto operator=(const MsaaAttachmentGroup&) -> MsaaAttachmentGroup& = delete;
        auto operator=(MsaaAttachmentGroup&&) noexcept -> MsaaAttachmentGroup& = default;
        ~MsaaAttachmentGroup() override = default;

        auto addColorAttachment(
            const vk::raii::Device &device,
            const Image &image,
            const Image &resolveImage,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
            const vk::ImageSubresourceRange &resolveSubresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        ) -> const MsaaAttachment& {
            return colorAttachments.emplace_back(
                Attachment {
                    image,
                    vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                        {},
                        image,
                        vk::ImageViewType::e2D,
                        format == vk::Format::eUndefined ? image.format : format,
                        {},
                        subresourceRange,
                    } },
                },
                resolveImage,
                vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                    {},
                    resolveImage,
                    vk::ImageViewType::e2D,
                    format == vk::Format::eUndefined ? resolveImage.format : format,
                    {},
                    resolveSubresourceRange,
                } }
            );
        }

        [[nodiscard]] auto createColorImage(
            vma::Allocator allocator,
            vk::Format format,
            vk::ImageUsageFlags usage = {},
            const vma::AllocationCreateInfo &allocationCreateInfo = { {}, vma::MemoryUsage::eAutoPreferDevice }
        ) const -> AllocatedImage {
            return createAttachmentImage(allocator, format, sampleCount, usage | vk::ImageUsageFlagBits::eColorAttachment, allocationCreateInfo);
        }

        [[nodiscard]] auto createResolveImage(
            vma::Allocator allocator,
            vk::Format format,
            vk::ImageUsageFlags usage = {},
            const vma::AllocationCreateInfo &allocationCreateInfo = { {}, vma::MemoryUsage::eAutoPreferDevice }
        ) const -> AllocatedImage {
            return createAttachmentImage(allocator, format, vk::SampleCountFlagBits::e1, usage | vk::ImageUsageFlagBits::eColorAttachment, allocationCreateInfo);
        }

        auto setDepthAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return depthStencilAttachment.emplace(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        auto setStencilAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return depthStencilAttachment.emplace(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        auto setDepthStencilAttachment(
            const vk::raii::Device &device,
            const Image &image,
            vk::Format format = {},
            const vk::ImageSubresourceRange &subresourceRange = { vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1 }
        ) -> const Attachment& {
            return depthStencilAttachment.emplace(image, vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                format == vk::Format::eUndefined ? image.format : format,
                {},
                subresourceRange,
            } });
        }

        [[nodiscard]] auto createDepthStencilImage(
            vma::Allocator allocator,
            vk::Format format,
            vk::ImageUsageFlags usage = {},
            const vma::AllocationCreateInfo &allocationCreateInfo = { {}, vma::MemoryUsage::eAutoPreferDevice }
        ) const -> AllocatedImage {
            return createAttachmentImage(allocator, format, sampleCount, usage | vk::ImageUsageFlagBits::eDepthStencilAttachment, allocationCreateInfo);
        }


        auto getRenderingInfo(
            std::span<const std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearColorValue>> colorAttachmentInfos = {},
            std::optional<std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearDepthStencilValue>> depthStencilAttachmentInfo = {}
        ) const -> RefHolder<vk::RenderingInfo, std::vector<vk::RenderingAttachmentInfo>, std::optional<vk::RenderingAttachmentInfo>> override {
            assert(colorAttachments.size() == colorAttachmentInfos.size() && "Color attachment info count mismatch");
            assert(depthStencilAttachment.has_value() == depthStencilAttachmentInfo.has_value() && "Depth-stencil attachment info mismatch");
            return {
                [this](std::span<const vk::RenderingAttachmentInfo> colorAttachmentInfos, const std::optional<vk::RenderingAttachmentInfo> &depthStencilAttachmentInfo) {
                    return vk::RenderingInfo {
                        {},
                        { { 0, 0 }, extent },
                        1,
                        {},
                        colorAttachmentInfos,
                        depthStencilAttachmentInfo ? &*depthStencilAttachmentInfo : nullptr,
                    };
                },
                std::views::zip_transform([](const MsaaAttachment &attachment, const std::tuple<vk::AttachmentLoadOp, vk::AttachmentStoreOp, vk::ClearColorValue> &info) {
                    return vk::RenderingAttachmentInfo {
                        *attachment.view, vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ResolveModeFlagBits::eAverage, *attachment.resolveView, vk::ImageLayout::eColorAttachmentOptimal,
                        std::get<0>(info), std::get<1>(info), std::get<2>(info),
                    };
                }, colorAttachments, colorAttachmentInfos) | std::ranges::to<std::vector>(),
                depthStencilAttachment.transform([info = *depthStencilAttachmentInfo](const Attachment &attachment) {
                    return vk::RenderingAttachmentInfo {
                        *attachment.view, vk::ImageLayout::eDepthStencilAttachmentOptimal,
                        {}, {}, {},
                        std::get<0>(info), std::get<1>(info), std::get<2>(info),
                    };
                }),
            };
        }
    };
}