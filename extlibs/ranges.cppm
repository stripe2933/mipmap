module;

#include <version>

export module ranges;

export import std;

#define INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t ...Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})
#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#if __cpp_static_call_operator >= 202207L
#define STATIC_CALL_OPERATOR_STATIC static
#define STATIC_CALL_OPERATOR_CONST
#else
#define STATIC_CALL_OPERATOR_STATIC
#define STATIC_CALL_OPERATOR_CONST const
#endif

namespace ranges::views {
    struct enumerate_fn : std::ranges::range_adaptor_closure<enumerate_fn> {
        template <std::ranges::input_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return std::views::zip(std::views::iota(static_cast<std::ranges::range_difference_t<R>>(0)), FWD(r));
        }
    };
#if __cpp_lib_ranges_enumerate >= 202302L
    [[deprecated("Use std::views::enumerate instead.")]]
#endif
    export constexpr enumerate_fn enumerate;

    template <std::size_t N>
    struct adjacent_fn : std::ranges::range_adaptor_closure<adjacent_fn<N>> {
        template <std::ranges::forward_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return INDEX_SEQ(Is, N, {
                return std::views::zip(r | std::views::drop(Is)...);
            });
        }
    };
#if __cpp_lib_ranges_zip >= 202110L
    [[deprecated("Use std::views::adjacent instead.")]]
#endif
    export template <std::size_t N> constexpr adjacent_fn<N> adjacent;
#if __cpp_lib_ranges_zip >= 202110L
    [[deprecated("Use std::views::pairwise instead.")]]
#endif
    export constexpr adjacent_fn<2> pairwise;

    struct deref_fn : std::ranges::range_adaptor_closure<deref_fn> {
        STATIC_CALL_OPERATOR_STATIC constexpr auto operator()(
            std::ranges::input_range auto &&r
        ) STATIC_CALL_OPERATOR_CONST {
            return FWD(r) | std::views::transform([](auto &&x) -> decltype(auto) {
                return *FWD(x);
            });
        }
    };
    export constexpr deref_fn deref;
}