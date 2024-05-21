#pragma once

#include <ranges>
#include <version>

#define RANGES_INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t... Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})
#define RANGES_FWD(...) static_cast<decltype(__VA_ARGS__) &&>(__VA_ARGS__)

namespace ranges {
    template <typename Derived>
    struct range_adaptor_closure {
        template <std::ranges::range R>
        friend constexpr auto operator|(
            R &&r,
            const Derived &derived
        ) noexcept(std::is_nothrow_invocable_v<const Derived&, R>) {
            return derived(RANGES_FWD(r));
        }
    };

namespace views {
#if __cpp_lib_ranges_enumerate >= 202302L
    inline constexpr decltype(std::views::enumerate) enumerate;
#else
    struct enumerate_fn : range_adaptor_closure<enumerate_fn> {
        template <std::ranges::input_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return std::views::zip(std::views::iota(static_cast<std::ranges::range_difference_t<R>>(0)), RANGES_FWD(r));
        }
    };
    inline constexpr enumerate_fn enumerate;
#endif

#if __cpp_lib_ranges_zip >= 202110L
    template <std::size_t N>
    inline constexpr decltype(std::views::adjacent<N>) adjacent;
    inline constexpr decltype(std::views::pairwise) pairwise;
#else
    template <std::size_t N>
    struct adjacent_fn : range_adaptor_closure<adjacent_fn<N>> {
        template <std::ranges::forward_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return RANGES_INDEX_SEQ(Is, N, {
                return std::views::zip(r | std::views::drop(Is)...);
            });
        }
    };
    template <std::size_t N>
    inline constexpr adjacent_fn<N> adjacent;
    constexpr adjacent_fn<2> pairwise;
#endif

    struct deref_fn : range_adaptor_closure<deref_fn> {
        constexpr auto operator()(
            std::ranges::input_range auto &&r
        ) const {
            return RANGES_FWD(r) | std::views::transform([](auto &&x) -> decltype(auto) {
                return *RANGES_FWD(x);
            });
        }
    };
    constexpr deref_fn deref;
}
}