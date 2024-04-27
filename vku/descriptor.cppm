export module vku:descriptor;

import std;
export import vulkan_hpp;
import :details;
import :utils;

namespace vku {
    export template <std::size_t... BindingCounts>
    class DescriptorSetLayouts : public std::array<vk::DescriptorSetLayout, sizeof...(BindingCounts)> {
    public:
        template <std::size_t BindingCount, bool HasBindingFlags = false>
        struct LayoutBindings {
            std::array<vk::DescriptorSetLayoutBinding, BindingCount> bindings;
            vk::DescriptorSetLayoutCreateFlags flags = {};
        };

        template <std::size_t BindingCount>
        struct LayoutBindings<BindingCount, true> {
            std::array<vk::DescriptorSetLayoutBinding, BindingCount> bindings;
            vk::DescriptorSetLayoutCreateFlags flags = {};
            std::array<vk::DescriptorBindingFlags, BindingCount> bindingFlags;
        };

        // Type deductions.
        // TODO: tricky deduction, might be ill-formed...
        template <typename... Ts>
        LayoutBindings(Ts...) -> LayoutBindings<
            details::leading_n<vk::DescriptorSetLayoutBinding, Ts...>,
            std::convertible_to<details::last_type<Ts...>, std::array<vk::DescriptorBindingFlags, details::leading_n<vk::DescriptorSetLayoutBinding, Ts...>>>>;

        /// Number of descriptor sets in the descriptor set layouts.
        static constexpr std::size_t setCount = sizeof...(BindingCounts);

        /// Number of bindings in the descriptor set at the specified index.
        template <std::size_t SetIndex>
        static constexpr std::size_t bindingCount = get<SetIndex>(std::array { BindingCounts... }); // TODO.CXX26: use pack indexing.

        std::tuple<std::array<vk::DescriptorSetLayoutBinding, BindingCounts>...> setLayouts;

        template <bool... HasBindingFlags>
        explicit DescriptorSetLayouts(
            const vk::raii::Device &device,
            const LayoutBindings<BindingCounts, HasBindingFlags> &...layoutBindings
        ) : setLayouts { layoutBindings.bindings... },
            raiiLayouts { vk::raii::DescriptorSetLayout { device, getDescriptorSetLayoutCreateInfo(layoutBindings).get() }... } {
            static_cast<std::array<vk::DescriptorSetLayout, setCount>&>(*this)
                = std::apply([&](const auto &...x) { return std::array { *x... }; }, raiiLayouts);
        }

    private:
        std::array<vk::raii::DescriptorSetLayout, setCount> raiiLayouts;

        template <std::size_t BindingCount, bool HasBindingFlags>
        [[nodiscard]] static auto getDescriptorSetLayoutCreateInfo(
            const LayoutBindings<BindingCount, HasBindingFlags> &layoutBindings
        ) noexcept {
            if constexpr (HasBindingFlags) {
                return vk::StructureChain {
                    vk::DescriptorSetLayoutCreateInfo { layoutBindings.flags, layoutBindings.bindings },
                    vk::DescriptorSetLayoutBindingFlagsCreateInfo { layoutBindings.bindingFlags },
                };
            }
            else {
                return vk::StructureChain {
                    vk::DescriptorSetLayoutCreateInfo { layoutBindings.flags, layoutBindings.bindings },
                };
            }
        }
    };

    export template <details::derived_from_value_specialization_of<DescriptorSetLayouts> Layouts>
    class DescriptorSets : public std::array<vk::DescriptorSet, Layouts::setCount> {
    public:
        DescriptorSets(
            vk::Device device,
            vk::DescriptorPool descriptorPool,
            const Layouts &layouts
        ) : std::array<vk::DescriptorSet, Layouts::setCount> { device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                descriptorPool,
                layouts,
            }) | ranges::to_array<Layouts::setCount>() },
            layouts { layouts } { }

        // For push descriptor usage.
        explicit DescriptorSets(
            const Layouts &layouts
        ) : layouts { layouts } { }

        template <typename Self>
        [[nodiscard]] static auto allocateMultiple(
            vk::Device device,
            vk::DescriptorPool descriptorPool,
            const Layouts &descriptorSetLayouts,
            std::size_t n
        ) -> std::vector<Self> {
            const std::vector multipleSetLayouts
                // TODO: 1. Why pipe syntax between std::span and std::views::repeat not works? 2. Why compile-time sized span not works?
                = std::views::repeat(std::span<const vk::DescriptorSetLayout> { descriptorSetLayouts }, n)
                | std::views::join
                | std::ranges::to<std::vector>();

            const std::vector descriptorSets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                descriptorPool,
                multipleSetLayouts,
            });
            // TODO.CXX23: use std::views::chunk instead, like:
            // return descriptorSets
            //     | std::views::chunk(Layouts::setCount)
            //     | std::views::transform([&](const auto &sets) {
            //         return Self { descriptorSetLayouts, sets | ranges::to_array<Layouts::setCount>() };
            //     })
            //     | std::ranges::to<std::vector>();
            return std::views::iota(0UZ, n)
                | std::views::transform([&](std::size_t i) {
                    return Self {
                        descriptorSetLayouts,
                        std::views::counted(descriptorSets.data() + i * Layouts::setCount, Layouts::setCount)
                            | ranges::to_array<Layouts::setCount>(),
                    };
                })
                | std::ranges::to<std::vector>();
        }

    protected:
        template <std::uint32_t Set, std::uint32_t Binding>
        [[nodiscard]] auto getDescriptorWrite() const -> vk::WriteDescriptorSet {
            return {
                get<Set>(*this), // Error in here: you specify set index that exceeds the number of descriptor set layouts.
                Binding,
                0,
                {},
                get<Binding>(get<Set>(layouts.setLayouts)).descriptorType, // Error in here: you specify binding index that exceeds the number of layout bindings in the set.
            };
        }

    private:
        const Layouts &layouts;

        // For allocateMultiple.
        DescriptorSets(
            const Layouts &layouts,
            const std::array<vk::DescriptorSet, Layouts::setCount> &descriptorSets
        ) : std::array<vk::DescriptorSet, Layouts::setCount> { descriptorSets },
            layouts { layouts } { }
    };
}