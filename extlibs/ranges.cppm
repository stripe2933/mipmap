export module ranges;

export import std;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace ranges::views {
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