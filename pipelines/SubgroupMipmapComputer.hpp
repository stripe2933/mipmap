#pragma once

#include <vku/DescriptorSetLayouts.hpp>
#include <vku/DescriptorSets.hpp>
#include <vku/pipelines.hpp>
#include <vku/RefHolder.hpp>

#ifdef NDEBUG
#include <resources/shaders.hpp>
#endif

#define FWD(...) static_cast<decltype(__VA_ARGS__) &&>(__VA_ARGS__)

/**
 * Compute image mipmaps using subgroup shuffle operation. More efficient than MipmapComputer.
 *
 * @code
 * // Create pipeline and corresponding descriptor sets.
 * SubgroupMipmapComputer subgroupMipmapComputer { device, mipImageCount, subgroupSize }; // mipImageCount = targetImage.mipLevels
 * SubgroupMipmapComputer::DescriptorSets descriptorSets { device, descriptorPool, subgroupMipmapComputer.descriptorSetLayouts };
 *
 * // Update descriptorSets with image's mip views.
 * device.updateDescriptorSets(
 *     descriptorSets.getDescriptorWrites0(imageMipViews | ranges::views::deref).get(),
 *     {});
 *
 * // Execute compute shader.
 * // Image layout must be VK_IMAGE_LAYOUT_GENERAL.
 * subgroupMipmapComputer.compute(commandBuffer, descriptorSets, baseImageExtent, targetImage.mipLevels); // baseImageExtent = targetImage.extent
 * @endcode
 */
class SubgroupMipmapComputer {
public:
    struct DescriptorSetLayouts : vku::DescriptorSetLayouts<1> {
        explicit DescriptorSetLayouts(
            const vk::raii::Device &device,
            std::uint32_t mipImageCount
        ) : vku::DescriptorSetLayouts<1> { device, LayoutBindings {
            vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            vk::DescriptorSetLayoutBinding { 0, vk::DescriptorType::eStorageImage, mipImageCount, vk::ShaderStageFlagBits::eCompute },
            std::array { vku::toFlags(vk::DescriptorBindingFlagBits::eUpdateAfterBind) },
        } } { }
    };

    struct DescriptorSets : vku::DescriptorSets<DescriptorSetLayouts> {
        using vku::DescriptorSets<DescriptorSetLayouts>::DescriptorSets;

        [[nodiscard]] auto getDescriptorWrites0(
            auto &&mipImageViews
        ) const noexcept {
            return vku::RefHolder {
                [this](std::span<const vk::DescriptorImageInfo> imageInfos) {
                    return std::array {
                        getDescriptorWrite<0, 0>().setImageInfo(imageInfos),
                    };
                },
                FWD(mipImageViews)
                    | std::views::transform([](vk::ImageView imageView) {
                        return vk::DescriptorImageInfo { {}, imageView, vk::ImageLayout::eGeneral };
                    })
                    | std::ranges::to<std::vector>(),
            };
        }
    };

    struct PushConstant {
        std::uint32_t baseLevel;
        std::uint32_t remainingMipLevels;
    };

    DescriptorSetLayouts descriptorSetLayouts;
    vk::raii::PipelineLayout pipelineLayout;
    vk::raii::Pipeline pipeline;

    explicit SubgroupMipmapComputer(
        const vk::raii::Device &device,
        std::uint32_t mipImageCount,
        std::uint32_t subgroupSize
    ) : descriptorSetLayouts { device, mipImageCount },
        pipelineLayout { createPipelineLayout(device) },
        pipeline { createPipeline(device, subgroupSize) } { }

    auto compute(
        vk::CommandBuffer commandBuffer,
        const DescriptorSets &descriptorSets,
        const vk::Extent2D &baseImageExtent,
        std::uint32_t mipLevels
    ) const -> void {
        // Base image size must be greater than or equal to 32. Therefore, the first execution may process less than 5 mip levels.
        // For example, if base extent is 4096x4096 (mipLevels=13),
        // Step 0 (4096 -> 1024)
        // Step 1 (1024 -> 32)
        // Step 2 (32 -> 1) (full processing required)

        // TODO.CXX23: use std::views::chunk instead, like:
        // const std::vector indexChunks
        //     = std::views::iota(1U, targetImage.mipLevels)                             // [1, 2, ..., 11, 12]
        //     | std::views::reverse                                                     // [12, 11, ..., 2, 1]
        //     | std::views::chunk(5)                                                    // [[12, 11, 10, 9, 8], [7, 6, 5, 4, 3], [2, 1]]
        //     | std::views::transform([](auto &&chunk) {
        //          return chunk | std::views::reverse | std::ranges::to<std::vector>();
        //     })                                                                        // [[8, 9, 10, 11, 12], [3, 4, 5, 6, 7], [1, 2]]
        //     | std::views::reverse                                                     // [[1, 2], [3, 4, 5, 6, 7], [8, 9, 10, 11, 12]]
        //     | std::ranges::to<std::vector>();
        std::vector<std::vector<std::uint32_t>> indexChunks;
        for (int endMipLevel = mipLevels; endMipLevel > 1; endMipLevel -= 5) {
            indexChunks.emplace_back(
                std::views::iota(
                    static_cast<std::uint32_t>(std::max(1, endMipLevel - 5)),
                    static_cast<std::uint32_t>(endMipLevel))
                | std::ranges::to<std::vector>());
        }
        std::ranges::reverse(indexChunks);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSets, {});
        for (const auto &[idx, mipIndices] : indexChunks | ranges::views::enumerate) {
            if (idx != 0) {
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                    {},
                    vk::MemoryBarrier {
                        vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
                    },
                    {}, {});
            }

            commandBuffer.pushConstants<PushConstant>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, PushConstant {
                mipIndices.front() - 1U,
                static_cast<std::uint32_t>(mipIndices.size()),
            });
            commandBuffer.dispatch(
                (baseImageExtent.width >> mipIndices.front()) / 16U,
                (baseImageExtent.height >> mipIndices.front()) / 16U,
                1);
        }
    }

private:
    [[nodiscard]] auto createPipelineLayout(
        const vk::raii::Device &device
    ) const -> vk::raii::PipelineLayout {
        constexpr vk::PushConstantRange pushConstantRange {
            vk::ShaderStageFlagBits::eCompute,
            0, sizeof(PushConstant),
        };
        return { device, vk::PipelineLayoutCreateInfo {
            {},
            descriptorSetLayouts,
            pushConstantRange,
        } };
    }

    [[nodiscard]] auto createPipeline(
        const vk::raii::Device &device,
        std::uint32_t subgroupSize
    ) const -> vk::raii::Pipeline {
        const auto [_, stages] = vku::createStages(
            device,
            vku::Shader { vk::ShaderStageFlagBits::eCompute,
#ifdef NDEBUG
                vku::Shader::convert([=] {
                    switch (subgroupSize) {
                        case 8U:   return resources::shaders_subgroup_mipmap_8_comp();
                        case 16U:  return resources::shaders_subgroup_mipmap_16_comp();
                        case 32U:  return resources::shaders_subgroup_mipmap_32_comp();
                        case 64U:  return resources::shaders_subgroup_mipmap_64_comp();
                        case 128U: return resources::shaders_subgroup_mipmap_128_comp();
                        default:   throw std::runtime_error { "Subgroup size must be ≥ 8." };
                    }
                }()),
#else
                vku::Shader::readCode(std::format("shaders/subgroup_mipmap_{}.comp.spv", subgroupSize)),
#endif
            });
        return { device, nullptr, vk::ComputePipelineCreateInfo {
            {},
            get<0>(stages),
            *pipelineLayout,
        } };
    }
};