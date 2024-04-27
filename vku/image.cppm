export module vku:image;

import std;
export import vulkan_hpp;
export import :allocator;
import :utils;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace vku {
    export struct Image {
        vk::Image image;
        vk::Extent3D extent;
        vk::Format format;
        std::uint32_t mipLevels;
        std::uint32_t arrayLayers;

        constexpr operator vk::Image() const noexcept {
            return image;
        }

        [[nodiscard]] constexpr auto maxMipLevels() const noexcept -> vk::Extent2D {
            return maxMipLevels(convertExtent2D(extent));
        }

        [[nodiscard]] constexpr auto mipExtent(
            std::uint32_t mipLevel
        ) const noexcept -> vk::Extent2D {
            return mipExtent(convertExtent2D(extent), mipLevel);
        }

        [[nodiscard]] static auto maxMipLevels(
            const vk::Extent2D &extent
        ) noexcept -> std::uint32_t {
            constexpr auto ilog2 = []<std::unsigned_integral T>(T n){
                return std::bit_width(n) - 1;
            };
            return ilog2(std::min(extent.width, extent.height)) + 1U;
        }

        [[nodiscard]] static auto mipExtent(
            const vk::Extent2D &extent,
            std::uint32_t mipLevel
        ) noexcept -> vk::Extent2D {
            return { extent.width >> mipLevel, extent.height >> mipLevel };
        }
    };

    export struct AllocatedImage : Image {
        vma::Allocator allocator;
        vma::Allocation allocation;

        AllocatedImage(
            vma::Allocator _allocator,
            const vk::ImageCreateInfo &createInfo,
            const vma::AllocationCreateInfo &allocationCreateInfo
        ) : Image { nullptr, createInfo.extent, createInfo.format, createInfo.mipLevels, createInfo.arrayLayers },
            allocator { _allocator } {
            std::tie(image, allocation) = allocator.createImage(createInfo, allocationCreateInfo);
        }
        AllocatedImage(const AllocatedImage&) = delete;
        AllocatedImage(AllocatedImage &&src) noexcept
            : Image { static_cast<Image>(src) },
              allocator { src.allocator },
              allocation { std::exchange(src.allocation, nullptr) } { }
        auto operator=(const AllocatedImage&) -> AllocatedImage& = delete;
        auto operator=(AllocatedImage &&src) noexcept -> AllocatedImage& {
            if (allocation) {
                allocator.destroyImage(image, allocation);
            }

            static_cast<Image&>(*this) = static_cast<Image>(src);
            allocator = src.allocator;
            allocation = std::exchange(src.allocation, nullptr);
            return *this;
        }
        ~AllocatedImage() {
            if (allocation) {
                allocator.destroyImage(image, allocation);
            }
        }
    };

    export struct MappedImage : AllocatedImage {
        void *data;

        MappedImage(
            AllocatedImage image
        ) : AllocatedImage { std::move(image) },
            data { allocator.mapMemory(allocation) } { }
        MappedImage(const MappedImage&) = delete;
        MappedImage(MappedImage &&src) noexcept = default;
        auto operator=(const MappedImage&) -> MappedImage& = delete;
        auto operator=(MappedImage &&src) noexcept -> MappedImage& {
            if (allocation) {
                allocator.unmapMemory(allocation);
            }
            static_cast<AllocatedImage&>(*this) = static_cast<AllocatedImage&&>(std::move(src));
            data = src.data;
            return *this;
        }
        ~MappedImage() {
            if (allocation) {
                allocator.unmapMemory(allocation);
            }
        }

        template <typename R>
            requires std::ranges::input_range<R>
        static auto fromRange(
            vma::Allocator allocator,
            R &&r,
            vk::Format format,
            const vk::Extent3D &extent,
            vk::ImageUsageFlagBits usage,
            const vma::AllocationCreateInfo &allocationCreateInfo = {
                vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                vma::MemoryUsage::eAuto,
            }
        ) -> MappedImage {
            MappedImage result = AllocatedImage { allocator, vk::ImageCreateInfo {
                {},
                vk::ImageType::e2D,
                format,
                extent,
                1, 1,
                vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eLinear,
                usage,
                {}, {},
                vk::ImageLayout::ePreinitialized,
            }, allocationCreateInfo };
            std::ranges::copy(FWD(r), static_cast<std::ranges::range_value_t<R>*>(result.data));
            return result;
        }
    };

    export
    [[nodiscard]] constexpr auto fullSubresourceRange(
        vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor
    ) noexcept -> vk::ImageSubresourceRange {
        return { aspectFlags, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers };
    }
}
