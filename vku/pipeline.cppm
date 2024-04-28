module;

#include <cerrno>

export module vku:pipeline;

import std;
import std.compat;
export import vulkan_hpp;
import :details;

#define INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t... Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})
#define ARRAY_OF(N, ...)                            \
    INDEX_SEQ(Is, N, {                              \
        return std::array { (Is, __VA_ARGS__)... }; \
    })

namespace vku {
    export struct Shader {
        vk::ShaderStageFlagBits stage;
        std::span<const std::uint32_t> code;
        const char *entryPoint = "main";
        std::optional<vk::SpecializationInfo> specializationInfo;

        static auto readCode(
            const std::filesystem::path &path
        ) -> std::vector<std::uint32_t> {
            std::ifstream file { path, std::ios::ate | std::ios::binary };
            if (!file.is_open()) {
                throw std::runtime_error { std::format("Failed to open file {} ({})", path.c_str(), strerror(errno)) };
            }

            const std::size_t fileSizeInBytes = file.tellg();
            std::vector<std::uint32_t> buffer(fileSizeInBytes / sizeof(std::uint32_t));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(buffer.data()), fileSizeInBytes);

            return buffer;
        }

        template <typename T>
            requires (4 % sizeof(T) == 0)
        [[nodiscard]] static auto convert(
            std::span<const T> data
        ) noexcept -> std::span<const std::uint32_t> {
            return { reinterpret_cast<const std::uint32_t*>(data.data()), data.size() * sizeof(T) / sizeof(std::uint32_t) };
        }
    };

    template <typename T>
    constexpr std::array<vk::SpecializationMapEntry, details::size<T>()> specializationMapEntries = [] {
        std::array<vk::SpecializationMapEntry, details::size<T>()> result;
        details::for_each([&](auto I) {
            get<I>(result)
                .setConstantID(I)
                .setOffset(details::offset_of<I, T>())
                .setSize(details::size_of<I, T>());
        }, result);
        return result;
    }();

    export template <typename T>
    [[nodiscard]] auto getSpecializationInfo(
        const T &specializationData
    ) -> vk::SpecializationInfo {
        return { specializationMapEntries<T>, vk::ArrayProxyNoTemporaries(specializationData) };
    }

    export template <std::convertible_to<Shader>... Shaders>
    [[nodiscard]] auto createStages(
        const vk::raii::Device &device,
        const Shaders &...shaders
    ) -> std::pair<std::array<vk::raii::ShaderModule, sizeof...(Shaders)>, std::array<vk::PipelineShaderStageCreateInfo, sizeof...(Shaders)>> {
        std::pair result {
            std::array { vk::raii::ShaderModule { device, vk::ShaderModuleCreateInfo {
                {},
                shaders.code,
            } }... },
            std::array { vk::PipelineShaderStageCreateInfo {
                {},
                shaders.stage,
                {},
                shaders.entryPoint,
                shaders.specializationInfo ? &*shaders.specializationInfo : nullptr,
            }... },
        };
        INDEX_SEQ(Is, sizeof...(Shaders), {
            (get<Is>(result.second).setModule(*get<Is>(result.first)), ...);
        });
        return result;
    }

    export
    [[nodiscard]] auto getDefaultGraphicsPipelineCreateInfo(
        vk::ArrayProxyNoTemporaries<const vk::PipelineShaderStageCreateInfo> stages,
        vk::PipelineLayout layout,
        std::uint32_t colorAttachmentCount = 0,
        bool hasDepthStencilAttachemnt = false,
        vk::SampleCountFlagBits multisample = vk::SampleCountFlagBits::e1
    ) -> vk::GraphicsPipelineCreateInfo {
        static constexpr vk::PipelineVertexInputStateCreateInfo vertexInputState{};

        static constexpr vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState {
            {},
            vk::PrimitiveTopology::eTriangleList,
        };

        static constexpr vk::PipelineViewportStateCreateInfo viewportState {
            {},
            1, {},
            1, {},
        };

        static constexpr vk::PipelineRasterizationStateCreateInfo rasterizationState {
            {},
            {}, {},
            vk::PolygonMode::eFill,
            vk::CullModeFlagBits::eBack, {},
            {}, {}, {}, {},
            1.f,
        };

        static constexpr std::array multisampleStates = std::apply([](auto ...args) {
            return std::array { vk::PipelineMultisampleStateCreateInfo { {}, args }... };
        }, std::array {
            vk::SampleCountFlagBits::e1,
            vk::SampleCountFlagBits::e2,
            vk::SampleCountFlagBits::e4,
            vk::SampleCountFlagBits::e8,
            vk::SampleCountFlagBits::e16,
            vk::SampleCountFlagBits::e32,
            vk::SampleCountFlagBits::e64,
        });

        static constexpr vk::PipelineDepthStencilStateCreateInfo depthStencilState{};

        constexpr std::uint32_t MAX_COLOR_ATTACHMENT_COUNT = 8;
        if (colorAttachmentCount > MAX_COLOR_ATTACHMENT_COUNT) {
            throw std::runtime_error { "Color attachment count exceeds maximum" };
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
        static constexpr std::array colorBlendAttachments = ARRAY_OF(MAX_COLOR_ATTACHMENT_COUNT + 1, vk::PipelineColorBlendAttachmentState {
            {},
            {}, {}, {},
            {}, {}, {},
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        });
#pragma clang diagnostic pop
        static constexpr std::array colorBlendStates = INDEX_SEQ(Is, MAX_COLOR_ATTACHMENT_COUNT + 1, {
            return std::array { vk::PipelineColorBlendStateCreateInfo {
                {},
                {}, {},
                Is, colorBlendAttachments.data(),
            }... };
        });

        static constexpr std::array dynamicStates {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };
        static constexpr vk::PipelineDynamicStateCreateInfo dynamicState {
            {},
            dynamicStates.size(), dynamicStates.data(),
        };

        return {
            {},
            stages,
            &vertexInputState,
            &inputAssemblyState,
            {},
            &viewportState,
            &rasterizationState,
            &multisampleStates[std::countr_zero(static_cast<std::underlying_type_t<vk::SampleCountFlagBits>>(multisample))],
            hasDepthStencilAttachemnt ? &depthStencilState : nullptr,
            &colorBlendStates[colorAttachmentCount],
            &dynamicState,
            layout,
        };
    }
}