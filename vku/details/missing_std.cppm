export module vku:details.missing_std;

export import std;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace std::ranges::views {
    struct enumerate_fn : range_adaptor_closure<enumerate_fn> {
        template <input_range R>
        static constexpr auto operator()(
            R &&r
        ) -> auto {
            return zip(iota(static_cast<range_difference_t<R>>(0)), FWD(r));
        }
    };
    export constexpr enumerate_fn enumerate;

    export constexpr auto zip_transform(
        auto &&f,
        input_range auto &&...rs
    ) -> auto {
        return zip(FWD(rs)...) | transform([&](auto &&t) {
            return apply(f, FWD(t));
        });
    }
}