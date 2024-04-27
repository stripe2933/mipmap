export module vku:utils;

import std;
#ifdef VKU_USE_GLM
import glm;
#endif
export import vulkan_hpp;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace vku{
    export template <typename T, typename... Ts>
    struct RefHolder {
        std::tuple<Ts...> temporaryValues;
        T value;

        template <std::invocable<Ts&...> F>
        RefHolder(F &&f, auto &&...temporaryValues)
            : temporaryValues { FWD(temporaryValues)... },
              value { std::apply(FWD(f), this->temporaryValues) } {

        }

        constexpr operator T&() noexcept {
            return value;
        }
        constexpr operator const T&() const noexcept {
            return value;
        }

        constexpr auto get() noexcept -> T& {
            return value;
        }
        constexpr auto get() const noexcept -> const T& {
            return value;
        }
    };

    template <typename F, typename... Ts>
    RefHolder(F &&, Ts &&...) -> RefHolder<std::invoke_result_t<F, Ts&...>, Ts...>;

export {
#ifdef VKU_USE_GLM
    [[nodiscard]] constexpr auto convertExtent2D(
        const glm::uvec2 &v
    ) noexcept -> vk::Extent2D {
        return { v.x, v.y };
    }
#endif

    [[nodiscard]] constexpr auto convertExtent2D(
        const vk::Extent3D &extent
    ) noexcept -> vk::Extent2D {
        return { extent.width, extent.height };
    }

    [[nodiscard]] constexpr auto convertOffset2D(
        const vk::Extent2D &extent
    ) noexcept -> vk::Offset2D {
        return { static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height) };
    }

    [[nodiscard]] constexpr auto convertOffset3D(
        const vk::Extent3D &extent
    ) noexcept -> vk::Offset3D {
        return { static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), static_cast<std::int32_t>(extent.depth) };
    }

    /**
     * @brief Get aspect ratio of \p vk::Extent2D rectangle.
     * @param extent
     * @return Aspect ratio, i.e. width / height.
     */
    [[nodiscard]] constexpr auto aspect(
        const vk::Extent2D &extent
    ) noexcept -> float {
        return static_cast<float>(extent.width) / extent.height;
    }

    [[nodiscard]] constexpr auto toFlags(auto flagBit) noexcept -> vk::Flags<decltype(flagBit)> {
        return flagBit;
    }

    template <typename T>
    [[nodiscard]] constexpr auto contains(vk::Flags<T> flags, T flag) noexcept -> bool {
        return (flags & flag) == flag;
    }

    template <typename T>
    [[nodiscard]] constexpr auto contains(vk::Flags<T> flags, vk::Flags<T> flag) noexcept -> bool {
        return (flags & flag) == flag;
    }
}
}