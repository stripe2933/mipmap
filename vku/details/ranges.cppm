export module vku:details.ranges;

export import std;

#define INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t ...Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})
#define ARRAY_OF(N, ...)                            \
    INDEX_SEQ(Is, N, {                              \
        return std::array { (Is, __VA_ARGS__)... }; \
    })
#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace ranges {
    export template <std::size_t N>
    struct to_array : std::ranges::range_adaptor_closure<to_array<N>> {
        template <std::ranges::input_range R>
        static constexpr auto operator()(
            R &&r
        ) -> std::array<std::ranges::range_value_t<R>, N> {
            auto it = r.begin();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
            return ARRAY_OF(N, *it++);
#pragma clang diagnostic pop
        }
    };

namespace views {
    struct deref_fn : std::ranges::range_adaptor_closure<deref_fn> {
        static constexpr auto operator()(
            std::ranges::input_range auto &&r
        ) {
            return FWD(r) | std::views::transform([](auto &&x) -> decltype(auto) {
                return *FWD(x);
            });
        }
    };
    export constexpr deref_fn deref;
}
}