export module missing_std;

export import std;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace std::ranges::views {
    struct enumerate_fn : range_adaptor_closure<enumerate_fn> {
        template <std::ranges::input_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return zip(iota(static_cast<range_difference_t<R>>(0)), FWD(r));
        }
    };
    export constexpr enumerate_fn enumerate;

    template <size_t N>
    struct adjacent_fn : range_adaptor_closure<adjacent_fn<N>> {
        template <forward_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return zip(r, r | drop(1));
        }
    };
    export template <size_t N> constexpr adjacent_fn<N> adjacent;
    export constexpr adjacent_fn<2> pairwise;

    export constexpr auto zip_transform(
        auto &&f,
        input_range auto &&...rs
    ) -> auto {
        return zip(FWD(rs)...) | transform([&](auto &&t) {
            return apply(f, FWD(t));
        });
    }

    export template <size_t N>
    constexpr auto adjacent_transform(
        auto &&f,
        forward_range auto &&r
    ) -> auto {
        return FWD(r) | adjacent<N> | transform([&](auto &&t) {
            return apply(f, FWD(t));
        });
    }
}