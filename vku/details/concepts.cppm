export module vku:details.concepts;

import std;

namespace vku::details{
    template <typename>
    struct is_tuple_like : std::false_type{};
    template <typename T, typename U>
    struct is_tuple_like<std::pair<T, U>> : std::true_type{};
    template <typename... Ts>
    struct is_tuple_like<std::tuple<Ts...>> : std::true_type{};
    template <typename T, std::size_t N>
    struct is_tuple_like<std::array<T, N>> : std::true_type{};
    export template <typename T>
    concept tuple_like = is_tuple_like<T>::value;

    // https://stackoverflow.com/questions/70130735/c-concept-to-check-for-derived-from-template-specialization
    template <template <auto...> typename Template, auto... Args>
    void derived_from_value_specialization_impl(const Template<Args...>&);

    export template <typename T, template <auto...> class Template>
    concept derived_from_value_specialization_of = requires(const T& t) {
        derived_from_value_specialization_impl<Template>(t);
    };

    export template <typename T, typename... Ts>
    constexpr std::size_t leading_n = (0 + ... + std::same_as<T, Ts>);

    export template <typename... Ts>
    using last_type = typename decltype((std::type_identity<Ts>{}, ...))::type; // TODO.CXX26: use pack indexing.
}