export module ranges;

export import std;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#if __cpp_static_call_operator >= 202207L
#define STATIC_CALL_OPERATOR_STATIC static
#define STATIC_CALL_OPERATOR_CONST
#else
#define STATIC_CALL_OPERATOR_STATIC
#define STATIC_CALL_OPERATOR_CONST const
#endif

namespace ranges::views {
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