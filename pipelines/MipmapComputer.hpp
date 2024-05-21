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
 * Compute image mipmaps.
 *
 * @code
 * // Create pipeline and corresponding descriptor sets.
 * MipmapComputer mipmapComputer { device, mipImageCount }; // mipImageCount = targetImage.mipLevels
 * MipmapComputer::DescriptorSets descriptorSets { device, descriptorPool, mipmapComputer.descriptorSetLayouts };
 *
 * // Update descriptorSets with image's mip views.
 * device.updateDescriptorSets(
 *     descriptorSets.getDescriptorWrites0(imageMipViews | ranges::views::deref).get(),
 *     {});
 *
 * // Execute compute shader.
 * // Image layout must be VK_IMAGE_LAYOUT_GENERAL.
 * mipmapComputer.compute(commandBuffer, descriptorSets, baseImageExtent, targetImage.mipLevels); // baseImageExtent = targetImage.extent
 * @endcode
 */
class MipmapComputer {
public:
    struct DescriptorSetLayouts : vku::DescriptorSetLayouts<1> {
        explicit DescriptorSetLayouts(
            const vk::raii::Device &device,
            std::uint32_t mipImageCount
        ) : vku::DescriptorSetLayouts<1> { device, LayoutBindings {
            vk::DescriptorSetLayoutBinding { 0, vk::DescriptorType::eStorageImage, mipImageCount, vk::ShaderStageFlagBits::eCompute },
            vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
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
    };

    DescriptorSetLayouts descriptorSetLayouts;
    vk::raii::PipelineLayout pipelineLayout;
    vk::raii::Pipeline pipeline;

    explicit MipmapComputer(
        const vk::raii::Device &device,
        std::uint32_t mipImageCount
    ) : descriptorSetLayouts { device, mipImageCount },
        pipelineLayout { createPipelineLayout(device) },
        pipeline { createPipeline(device) } { }

    auto compute(
        vk::CommandBuffer commandBuffer,
        const DescriptorSets &descriptorSets,
        const vk::Extent2D &baseImageExtent,
        std::uint32_t mipLevels
    ) const -> void {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSets, {});
        for (auto [srcLevel, dstLevel] : std::views::iota(0U, mipLevels) | ranges::views::pairwise) {
            if (srcLevel != 0U) {
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                    {},
                    vk::MemoryBarrier {
                        vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
                    },
                    {}, {});
            }

            commandBuffer.pushConstants<PushConstant>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, PushConstant { srcLevel });
            commandBuffer.dispatch(
                vku::divCeil(baseImageExtent.width >> dstLevel, 16U),
                vku::divCeil(baseImageExtent.height >> dstLevel, 16U),
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
        const vk::raii::Device &device
    ) const -> vk::raii::Pipeline {
        const auto [_, stages] = vku::createStages(
            device,
            vku::Shader { vk::ShaderStageFlagBits::eCompute,
#ifdef NDEBUG
                vku::Shader::convert(resources::shaders_mipmap_comp()),
#else
                vku::Shader::readCode("shaders/mipmap.comp.spv"),
#endif
            });
        return { device, nullptr, vk::ComputePipelineCreateInfo {
            {},
            get<0>(stages),
            *pipelineLayout,
        } };
    }
};