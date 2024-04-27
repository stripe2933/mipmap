export module vku:swapchain;

import std;
export import vulkan_hpp;
import :image;

namespace vku {
namespace details {
    template <typename T, typename... Ts>
    concept one_of = (std::same_as<T, Ts> || ...);
}

    export template <typename... PNexts>
    class Swapchain {
    public:
        Swapchain(
            const vk::raii::Device &device,
            const vk::StructureChain<vk::SwapchainCreateInfoKHR, PNexts...> &createInfo
        ) : device { device },
            info { createInfo },
            swapchain { device, info.get() } { }

        [[nodiscard]] auto acquireImage(
            vk::Semaphore imageAcquireSema = {},
            std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()
        ) const -> std::optional<std::uint32_t> {
            const auto [result, imageIndex]
                = (*device).acquireNextImageKHR(*swapchain, timeout, imageAcquireSema);
            switch (result) {
                case vk::Result::eSuccess: case vk::Result::eSuboptimalKHR:
                    return imageIndex;
                case vk::Result::eErrorOutOfDateKHR:
                    return std::nullopt;
                default:
                    throw std::runtime_error { "Acquiring swapchain image failed." };
            }
        }

        [[nodiscard]] auto getInfo() const noexcept -> const vk::SwapchainCreateInfoKHR & {
            return info.get();
        }

        [[nodiscard]] auto getFormat() const noexcept -> vk::Format {
            return info.get().imageFormat;
        }

        [[nodiscard]] auto getExtent() const noexcept -> vk::Extent2D {
            return info.get().imageExtent;
        }

        [[nodiscard]] auto getImages() const noexcept -> std::span<const vk::Image> {
            return images;
        }

        [[nodiscard]] auto presentImage(
            vk::Queue presentQueue,
            std::uint32_t imageIndex,
            vk::ArrayProxy<const vk::Semaphore> waitSemas = {}
        ) const -> bool {
            switch (presentQueue.presentKHR(vk::PresentInfoKHR { waitSemas, *swapchain, imageIndex })) {
                case vk::Result::eSuccess:
                    return true;
                case vk::Result::eErrorOutOfDateKHR: case vk::Result::eSuboptimalKHR:
                    return false;
                default:
                    throw std::runtime_error { "Presenting swapchain image failed." };
            }
        }

        auto changeExtent(
            vk::Extent2D extent
        ) -> void {
            info.get().imageExtent = extent;
            info.get().oldSwapchain = *swapchain;
            swapchain = { device, info.get() };
            images = swapchain.getImages();
        }

        [[nodiscard]] auto createImageViews() const -> std::vector<vk::raii::ImageView> {
            return createImageViewsWithFormat(getFormat());
        }

        [[nodiscard]] auto createImageViews(
            vk::Format format
        ) const -> std::vector<vk::raii::ImageView>
            requires details::one_of<vk::ImageFormatListCreateInfo, PNexts...>
        {
            return createImageViewsWithFormat(format);
        }

    private:
        const vk::raii::Device &device;
        vk::StructureChain<vk::SwapchainCreateInfoKHR, PNexts...> info;
        vk::raii::SwapchainKHR swapchain;
        std::vector<vk::Image> images = swapchain.getImages();

        [[nodiscard]] auto createImageViewsWithFormat(
            vk::Format format
        ) const -> std::vector<vk::raii::ImageView> {
            return images
                | std::views::transform([this, format](vk::Image image) {
                    return vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                        {},
                        image,
                        vk::ImageViewType::e2D,
                        format,
                        {},
                        fullSubresourceRange(),
                    } };
                }) | std::ranges::to<std::vector>();
        }
    };
}