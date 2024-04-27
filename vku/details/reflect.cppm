// https://github.com/boost-ext/reflect

export module vku:details.reflect;

import std;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)
#define FWD_LIKE(T, ...) static_cast<typename FwdLike<::std::is_lvalue_reference_v<T>>::template type<decltype(__VA_ARGS__)>>(__VA_ARGS__)
#define INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t... Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})

namespace vku::details {
    template <bool> struct FwdLike { template <class T> using type = std::remove_reference_t<T>&&; };
    template <> struct FwdLike<true> { template <class T> using type = std::remove_reference_t<T>&; }; // to speed up compilation times

    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&&,   std::integral_constant<std::size_t, 0>) noexcept { return FWD(fn)(); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 1>) noexcept { auto&& [_1] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 2>) noexcept { auto&& [_1, _2] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 3>) noexcept { auto&& [_1, _2, _3] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 4>) noexcept { auto&& [_1, _2, _3, _4] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 5>) noexcept { auto&& [_1, _2, _3, _4, _5] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 6>) noexcept { auto&& [_1, _2, _3, _4, _5, _6] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 7>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 8>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 9>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 10>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9), FWD_LIKE(T, _10)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 11>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9), FWD_LIKE(T, _10), FWD_LIKE(T, _11)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 12>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9), FWD_LIKE(T, _10), FWD_LIKE(T, _11), FWD_LIKE(T, _12)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 13>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9), FWD_LIKE(T, _10), FWD_LIKE(T, _11), FWD_LIKE(T, _12), FWD_LIKE(T, _13)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 14>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9), FWD_LIKE(T, _10), FWD_LIKE(T, _11), FWD_LIKE(T, _12), FWD_LIKE(T, _13), FWD_LIKE(T, _14)); }
    template <class Fn, class T> [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, std::integral_constant<std::size_t, 15>) noexcept { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15] = FWD(t); return FWD(fn)(FWD_LIKE(T, _1), FWD_LIKE(T, _2), FWD_LIKE(T, _3), FWD_LIKE(T, _4), FWD_LIKE(T, _5), FWD_LIKE(T, _6), FWD_LIKE(T, _7), FWD_LIKE(T, _8), FWD_LIKE(T, _9), FWD_LIKE(T, _10), FWD_LIKE(T, _11), FWD_LIKE(T, _12), FWD_LIKE(T, _13), FWD_LIKE(T, _14), FWD_LIKE(T, _15)); }

    template <class T> extern const T ext{};
    struct any { template <class T> operator T() const noexcept; };
    template<auto...> struct auto_ { constexpr explicit(false) auto_(auto&&...) noexcept { } };

    template <std::size_t N, class...Ts> requires (N < sizeof...(Ts))
    [[nodiscard]] constexpr decltype(auto) nth_pack_element(Ts&&...args) noexcept {
        return INDEX_SEQ(Ns, N, {
            return [](auto_<Ns>&&..., auto&& nth, auto&&...) -> decltype(auto) { return FWD(nth); };
        })(FWD(args)...);
    }

    template <class T, class... Ts>
        requires std::is_aggregate_v<T>
    [[nodiscard]] constexpr auto size() -> std::size_t {
        if constexpr (requires { T{Ts{}...}; } and not requires { T{Ts{}..., any{}}; }) {
              return sizeof...(Ts);
        } else {
              return size<T, Ts..., any>();
        }
    }

    export template <class Fn, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, auto...) noexcept {
#if __cpp_structured_bindings >= 202401L
        auto&& [... ts] = FWD(t);
          return FWD(fn)(FWD_LIKE(T, ts)...);
#else
          return visit(FWD(fn), FWD(t), std::integral_constant<std::size_t, size<std::remove_cvref_t<T>>()>{});
#endif
    }

    template<std::size_t N, class T> requires (std::is_aggregate_v<std::remove_cvref_t<T>> and N < size<std::remove_cvref_t<T>>())
    [[nodiscard]] constexpr decltype(auto) get(T&& t) noexcept {
        return visit([](auto&&... args) -> decltype(auto) { return nth_pack_element<N>(FWD(args)...); }, FWD(t));
    }

    export template <std::size_t N, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr auto size_of() -> std::size_t {
        return sizeof(std::remove_cvref_t<decltype(get<N>(ext<T>))>);
    }

    export template <std::size_t N, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr auto size_of(T&&) -> std::size_t {
        return size_of<N, std::remove_cvref_t<T>>();
    }

    export template <std::size_t N, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr auto align_of() -> std::size_t {
        return alignof(std::remove_cvref_t<decltype(get<N>(ext<T>))>);
    }

    export template <std::size_t N, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr auto align_of(T&&) -> std::size_t {
        return align_of<N, std::remove_cvref_t<T>>();
    }

    export template <std::size_t N, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr auto offset_of() -> std::size_t {
      if constexpr (not N) {
          return {};
      } else {
          constexpr auto offset = offset_of<N-1, T>() + size_of<N-1, T>();
          constexpr auto alignment = std::min(alignof(T), align_of<N, T>());
          constexpr auto padding = offset % alignment;
          return offset + padding;
      }
    }

    export template <std::size_t N, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    [[nodiscard]] constexpr auto offset_of(T&&) -> std::size_t {
        return offset_of<N, std::remove_cvref_t<T>>();
    }

    export template <class T, class Fn>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    constexpr auto for_each(Fn&& fn) -> void {
        INDEX_SEQ(Ns, size<std::remove_cvref_t<T>>(), {
          (FWD(fn)(std::integral_constant<decltype(Ns), Ns>{}), ...);
        });
    }

    export template <class Fn, class T>
        requires std::is_aggregate_v<std::remove_cvref_t<T>>
    constexpr auto for_each(Fn&& fn, T&&) -> void {
        INDEX_SEQ(Ns, size<std::remove_cvref_t<T>>(), {
          (FWD(fn)(std::integral_constant<decltype(Ns), Ns>{}), ...);
        });
    }
}
