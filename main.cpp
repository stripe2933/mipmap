#include <stb_image_write.h>

import std;
import image_data;
import missing_std;
import vku;

#define INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t... Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})
#define ARRAY_OF(N, ...)                            \
    INDEX_SEQ(Is, N, {                              \
        return std::array { (Is, __VA_ARGS__)... }; \
    })
#define FWD(...) static_cast<decltype(__VA_ARGS__) &&>(__VA_ARGS__)

template <std::unsigned_integral T>
constexpr auto divCeil(T num, T denom) noexcept -> T {
    return num / denom + num % denom;
}

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
        for (auto [srcLevel, dstLevel] : std::views::iota(0U, mipLevels) | std::views::pairwise) {
            if (srcLevel != 0U) {
                constexpr vk::MemoryBarrier barrier {
                    vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
                };
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                    {}, barrier, {}, {});
            }

            commandBuffer.pushConstants<PushConstant>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, PushConstant { srcLevel });
            commandBuffer.dispatch(
                divCeil(baseImageExtent.width >> dstLevel, 16U),
                divCeil(baseImageExtent.height >> dstLevel, 16U),
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
            vku::Shader { vk::ShaderStageFlagBits::eCompute, vku::Shader::readCode("shaders/mipmap.comp.spv") });
        return { device, nullptr, vk::ComputePipelineCreateInfo {
            {},
            get<0>(stages),
            *pipelineLayout,
        } };
    }
};

class SubgroupMipmapComputer {
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
        std::uint32_t remainingMipLevels;
    };

    DescriptorSetLayouts descriptorSetLayouts;
    vk::raii::PipelineLayout pipelineLayout;
    vk::raii::Pipeline pipeline;

    explicit SubgroupMipmapComputer(
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
        for (const auto &[idx, mipIndices] : indexChunks | std::views::enumerate) {
            if (idx != 0) {
                constexpr vk::MemoryBarrier barrier {
                    vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
                };
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                    {}, barrier, {}, {});
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
        const vk::raii::Device &device
    ) const -> vk::raii::Pipeline {
        const auto [_, stages] = vku::createStages(
            device,
            vku::Shader { vk::ShaderStageFlagBits::eCompute, vku::Shader::readCode("shaders/subgroup_mipmap.comp.spv") });
        return { device, nullptr, vk::ComputePipelineCreateInfo {
            {},
            get<0>(stages),
            *pipelineLayout,
        } };
    }
};

struct QueueFamilyIndices {
    std::uint32_t compute;
    std::uint32_t graphics;
    std::uint32_t transfer;

    explicit QueueFamilyIndices(
        vk::PhysicalDevice physicalDevice
    ) {
        const std::vector queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // Compute: prefer compute-only (\w transfer ok) queue family.
        if (auto it = std::ranges::find_if(queueFamilyProperties, [](vk::QueueFlags flags) {
            return (flags & ~vk::QueueFlagBits::eTransfer) == vk::QueueFlagBits::eCompute;
        }, &vk::QueueFamilyProperties::queueFlags); it != queueFamilyProperties.end()) {
            compute = it - queueFamilyProperties.begin();
        }
        else if (auto it = std::ranges::find_if(queueFamilyProperties, [](vk::QueueFlags flags) {
            return vku::contains(flags, vk::QueueFlagBits::eCompute);
        }, &vk::QueueFamilyProperties::queueFlags); it != queueFamilyProperties.end()) {
            compute = it - queueFamilyProperties.begin();
        }
        else {
            throw std::runtime_error { "Physical device doesn't have compute queue family" };
        }

        // Graphcs: any queue family is ok.
        if (auto it = std::ranges::find_if(queueFamilyProperties, [](vk::QueueFlags flags) {
            return vku::contains(flags, vk::QueueFlagBits::eGraphics);
        }, &vk::QueueFamilyProperties::queueFlags);
            it != queueFamilyProperties.end()) {
            graphics = it - queueFamilyProperties.begin();
        }
        else {
            throw std::runtime_error { "Physical device doesn't have graphics queue family" };
        }

        // Transfer: prefer transfer-only (\w sparse binding ok) queue fmaily.
        if (auto it = std::ranges::find_if(queueFamilyProperties, [](vk::QueueFlags flags) {
            return (flags & ~vk::QueueFlagBits::eSparseBinding) == vk::QueueFlagBits::eTransfer;
        }, &vk::QueueFamilyProperties::queueFlags);
            it != queueFamilyProperties.end()) {
            transfer = it - queueFamilyProperties.begin();
        }
        else if (auto it = std::ranges::find_if(queueFamilyProperties, [](vk::QueueFlags flags) {
            return vku::contains(flags, vk::QueueFlagBits::eTransfer);
        }, &vk::QueueFamilyProperties::queueFlags); it != queueFamilyProperties.end()) {
            transfer = it - queueFamilyProperties.begin();
        }
        else {
            throw std::runtime_error { "Physical device doesn't have transfer queue family" };
        }
    }
};

struct Queues {
    vk::Queue compute;
    vk::Queue graphics;
    vk::Queue transfer;

    Queues(
        vk::Device device,
        const QueueFamilyIndices &queueFamilyIndices
    ) : compute { device.getQueue(queueFamilyIndices.compute, 0) },
        graphics { device.getQueue(queueFamilyIndices.graphics, 0) },
        transfer { device.getQueue(queueFamilyIndices.transfer, 0) } { }

    [[nodiscard]] static auto getDeviceQueueCreateInfos(
        const QueueFamilyIndices &queueFamilyIndices
    ) -> std::vector<vk::DeviceQueueCreateInfo> {
        return std::set { queueFamilyIndices.graphics, queueFamilyIndices.compute, queueFamilyIndices.transfer }
            | std::views::transform([](std::uint32_t queueFamilyIndex) {
                static constexpr std::array queuePriorities { 1.f };
                return vk::DeviceQueueCreateInfo { {}, queueFamilyIndex, queuePriorities };
            })
            | std::ranges::to<std::vector>();
    }
};

class MainApp : vku::Instance, vku::Device<QueueFamilyIndices, Queues> {
public:
    vku::Allocator allocator = createAllocator();
    vk::raii::DescriptorPool descriptorPool = createDescriptorPool();
    vk::raii::CommandPool computeCommandPool = createCommandPool(queueFamilyIndices.compute),
                          graphicsCommandPool = createCommandPool(queueFamilyIndices.graphics),
                          transferCommandPool = createCommandPool(queueFamilyIndices.transfer);

    MainApp()
        : Instance { createInstance() },
          Device { createDevice() } { }

    auto run(
        const std::filesystem::path &imagePath,
        const std::filesystem::path &outputDir
    ) const -> void {
        const ImageData<std::uint8_t> imageData { imagePath, 4 };
        const vk::Extent2D baseImageExtent { static_cast<std::uint32_t>(imageData.width), static_cast<std::uint32_t>(imageData.height) };
        const std::uint32_t imageMipLevels = vku::Image::maxMipLevels(baseImageExtent);

        // Load image into staging buffer.
        const vku::MappedBuffer imageStagingBuffer = vku::MappedBuffer::fromRange(
            allocator,
            std::views::counted(imageData.data.get(), imageData.width * imageData.height * imageData.channels),
            vk::BufferUsageFlagBits::eTransferSrc /* staging src */);

        // Create 3 device-local images (each images have different usage).
        const auto createBaseImage = [&](vk::ImageUsageFlags usage) {
            return vku::AllocatedImage { allocator, vk::ImageCreateInfo {
                {},
                vk::ImageType::e2D,
                vk::Format::eR8G8B8A8Unorm,
                vk::Extent3D { baseImageExtent, 1 },
                imageMipLevels, 1,
                vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferDst /* staging dst */
                    | usage
                    | vk::ImageUsageFlagBits::eTransferSrc /* destaging src */,
            }, vma::AllocationCreateInfo {
                {},
                vma::MemoryUsage::eAutoPreferDevice,
            } };
        };
        const std::array baseImages {
            createBaseImage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst), // For blit-based mipmap generation.
            createBaseImage(vk::ImageUsageFlagBits::eStorage), // For compute shader mipmap generation with per-level barriers.
            createBaseImage(vk::ImageUsageFlagBits::eStorage), // For compute shader mipmap generation with subgroup operation.
        };

        // Staging from imageStagingBuffer to baseImages[0..3].
        vku::executeSingleCommand(*device, *transferCommandPool, queues.transfer, [&](vk::CommandBuffer commandBuffer) {
            const std::array barriers = std::apply([](const auto &...images) {
                return std::array {
                    vk::ImageMemoryBarrier {
                        {}, vk::AccessFlagBits::eTransferWrite,
                        {}, vk::ImageLayout::eTransferDstOptimal,
                        {}, {},
                        images,
                        { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
                    }...
                };
            }, baseImages);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                {}, {}, {}, barriers);

            for (const vku::Image &baseImage : baseImages) {
                commandBuffer.copyBufferToImage(
                    imageStagingBuffer,
                    baseImage, vk::ImageLayout::eTransferDstOptimal,
                    vk::BufferImageCopy {
                        0, 0, 0,
                        { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
                        { 0, 0, 0 },
                        baseImage.extent,
                    });
            }
        });
        queues.transfer.waitIdle();

        const vk::raii::QueryPool queryPool { device, vk::QueryPoolCreateInfo {
            {},
            vk::QueryType::eTimestamp,
            2,
        } };
        const auto printElapsedTime = [&queryPool, timestampPeriod = physicalDevice.getProperties().limits.timestampPeriod](std::string_view label) {
            const auto [result, timestamps] = queryPool.getResults<std::uint64_t>(
                0, 2, 2 * sizeof(std::uint64_t), sizeof(std::uint64_t), vk::QueryResultFlagBits::e64);
            if (result == vk::Result::eSuccess) {
                std::println("{}: {:.2f} us", label, (timestamps[1] - timestamps[0]) * timestampPeriod / 1e3f);
            }
            else {
                std::println(std::cerr, "Failed to get timestamp query: {}", to_string(result));
            }
        };

        // 1. Blit-based mipmap generation.
        {
            const vku::Image &targetImage = get<0>(baseImages);

            vku::executeSingleCommand(*device, *graphicsCommandPool, queues.graphics, [&](vk::CommandBuffer commandBuffer) {
                commandBuffer.resetQueryPool(*queryPool, 0, 2);
                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

                for (auto [srcLevel, dstLevel] : std::views::iota(0U, targetImage.mipLevels) | std::views::pairwise) {
                    const std::array barriers {
                        vk::ImageMemoryBarrier {
                            srcLevel == 0U ? vk::AccessFlagBits::eNone : vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                            {}, {},
                            targetImage,
                            { vk::ImageAspectFlagBits::eColor, srcLevel, 1, 0, 1 }
                        },
                        vk::ImageMemoryBarrier {
                            {}, vk::AccessFlagBits::eTransferWrite,
                            {}, vk::ImageLayout::eTransferDstOptimal,
                            {}, {},
                            targetImage,
                            { vk::ImageAspectFlagBits::eColor, dstLevel, 1, 0, 1 }
                        },
                    };
                    commandBuffer.pipelineBarrier(
                        srcLevel == 0U ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                        {}, {}, {}, barriers);

                    commandBuffer.blitImage(
                        targetImage, vk::ImageLayout::eTransferSrcOptimal,
                        targetImage, vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageBlit {
                            { vk::ImageAspectFlagBits::eColor, srcLevel, 0, 1 },
                            { vk::Offset3D{}, vk::Offset3D { vku::convertOffset2D(targetImage.mipExtent(srcLevel)), 1 } },
                            { vk::ImageAspectFlagBits::eColor, dstLevel, 0, 1 },
                            { vk::Offset3D{}, vk::Offset3D { vku::convertOffset2D(targetImage.mipExtent(dstLevel)), 1 } },
                        },
                        vk::Filter::eLinear);
                }

                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
            });
            queues.graphics.waitIdle();
            printElapsedTime("Blit based mipmap generation");
        }


        // 2. Compute shader mipmap generation with per-level barriers.
        {
            const vku::Image &targetImage = get<1>(baseImages);

            const MipmapComputer mipmapComputer { device, targetImage.mipLevels };
            const MipmapComputer::DescriptorSets descriptorSets { *device, *descriptorPool, mipmapComputer.descriptorSetLayouts };

            const std::vector imageMipViews
                = std::views::iota(0U, targetImage.mipLevels)
                | std::views::transform([&](std::uint32_t mipLevel) {
                    return vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                        {},
                        targetImage,
                        vk::ImageViewType::e2D,
                        targetImage.format,
                        {},
                        { vk::ImageAspectFlagBits::eColor, mipLevel, 1, 0, 1 },
                    } };
                })
                | std::ranges::to<std::vector>();

            // Update descriptor sets.
            device.updateDescriptorSets(
                descriptorSets.getDescriptorWrites0(imageMipViews | std::views::transform([](const auto &x) { return *x; })).get(),
                {});

            vku::executeSingleCommand(*device, *computeCommandPool, queues.compute, [&](vk::CommandBuffer commandBuffer) {
                commandBuffer.resetQueryPool(*queryPool, 0, 2);
                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

                {
                    const vk::ImageMemoryBarrier barrier {
                       {}, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                       {}, vk::ImageLayout::eGeneral,
                       {}, {},
                       targetImage,
                       vku::fullSubresourceRange(),
                   };
                   commandBuffer.pipelineBarrier(
                       vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader,
                       {}, {}, {}, barrier);
                }

                mipmapComputer.compute(commandBuffer, descriptorSets, baseImageExtent, targetImage.mipLevels);

                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
            });
            queues.compute.waitIdle();
            printElapsedTime("Compute shader mipmap generation with per-level barriers");
        }

        // 3. Compute shader mipmap generation with subgroups.
        {
            const vku::Image &targetImage = get<2>(baseImages);

            const SubgroupMipmapComputer subgroupMipmapComputer { device, targetImage.mipLevels };
            const SubgroupMipmapComputer::DescriptorSets descriptorSets { *device, *descriptorPool, subgroupMipmapComputer.descriptorSetLayouts };

            const std::vector imageMipViews
                = std::views::iota(0U, targetImage.mipLevels)
                | std::views::transform([&](std::uint32_t mipLevel) {
                    return vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                        {},
                        targetImage,
                        vk::ImageViewType::e2D,
                        targetImage.format,
                        {},
                        { vk::ImageAspectFlagBits::eColor, mipLevel, 1, 0, 1 },
                    } };
                })
                | std::ranges::to<std::vector>();

            // Update descriptor sets.
            device.updateDescriptorSets(
                descriptorSets.getDescriptorWrites0(imageMipViews | std::views::transform([](const auto &x) { return *x; })).get(),
                {});

            vku::executeSingleCommand(*device, *computeCommandPool, queues.compute, [&](vk::CommandBuffer commandBuffer) {
                commandBuffer.resetQueryPool(*queryPool, 0, 2);
                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

                {
                    const vk::ImageMemoryBarrier barrier {
                       {}, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                       {}, vk::ImageLayout::eGeneral,
                       {}, {},
                       targetImage,
                       vku::fullSubresourceRange(),
                   };
                   commandBuffer.pipelineBarrier(
                       vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader,
                       {}, {}, {}, barrier);
                }

                subgroupMipmapComputer.compute(commandBuffer, descriptorSets, baseImageExtent, targetImage.mipLevels);

                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
            });
            queues.compute.waitIdle();
            printElapsedTime("Compute shader mipmap generation with subgroup operation");
        }

        const vk::Extent2D destagingImageExtent { baseImageExtent.width * 3U / 2U, baseImageExtent.height };
        const std::array destagingBuffers
            = ARRAY_OF(3, vku::MappedBuffer { vku::AllocatedBuffer { allocator, vk::BufferCreateInfo {
                {},
                blockSize(vk::Format::eR8G8B8A8Unorm) * destagingImageExtent.width * destagingImageExtent.height,
                vk::BufferUsageFlagBits::eTransferDst /* destaging dst */,
            }, vma::AllocationCreateInfo {
                vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped,
                vma::MemoryUsage::eAuto,
            } } });

        // Copy from baseImages[0..3] to destagingBuffers[0..3].
        vku::executeSingleCommand(*device, *transferCommandPool, queues.transfer, [&](vk::CommandBuffer commandBuffer) {
            const std::array barriers = std::apply([](const auto &...images) {
                return std::array {
                    vk::ImageMemoryBarrier {
                        {}, vk::AccessFlagBits::eTransferRead,
                        {}, vk::ImageLayout::eTransferSrcOptimal,
                        {}, {},
                        images,
                        vku::fullSubresourceRange(),
                    }...
                };
            }, baseImages);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                {}, {}, {}, barriers);

            const std::vector copyRegions
                = std::views::iota(0U, imageMipLevels)
                | std::views::transform([&, bufferOffset = 0U](std::uint32_t mipLevel) mutable {
                    if (mipLevel == 1U) {
                        bufferOffset += blockSize(vk::Format::eR8G8B8A8Unorm) * baseImageExtent.width;
                    }
                    else if (mipLevel >= 2U) {
                        bufferOffset += blockSize(vk::Format::eR8G8B8A8Unorm) * destagingImageExtent.width * (destagingImageExtent.height >> (mipLevel - 1U));
                    }

                    return vk::BufferImageCopy {
                        bufferOffset, destagingImageExtent.width, destagingImageExtent.height,
                        { vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1 },
                        { 0, 0, 0 },
                        vk::Extent3D { vku::Image::mipExtent(baseImageExtent, mipLevel), 1 },
                    };
                })
                | std::ranges::to<std::vector>();

            for (const auto &[baseImage, destagingBuffer] : std::views::zip(baseImages, destagingBuffers)) {
                commandBuffer.copyImageToBuffer(
                    baseImage, vk::ImageLayout::eTransferSrcOptimal,
                    destagingBuffer,
                    copyRegions);
            }
        });
        queues.transfer.waitIdle();

        constexpr std::array fileBasenames { "blit", "compute_per_level_barriers", "compute_subgroup" };
        for (const auto &[destagingBuffer, fileBasename] : std::views::zip(destagingBuffers, fileBasenames)) {
            stbi_write_png((outputDir / fileBasename).replace_extension(".png").c_str(),
                destagingImageExtent.width, destagingImageExtent.height, 4,
                destagingBuffer.data, blockSize(vk::Format::eR8G8B8A8Unorm) * destagingImageExtent.width);
        }
    }

private:
    [[nodiscard]] auto createDevice() const -> Device {
        return Device { instance, Device::Config {
            .physicalDeviceRater = [](vk::PhysicalDevice physicalDevice) {
                if (const vk::PhysicalDeviceLimits limits = physicalDevice.getProperties().limits;
                    limits.timestampPeriod == 0.f || !limits.timestampComputeAndGraphics) {
                    // Timestamp query not supported.
                    return 0U;
                }

                const vk::StructureChain properties2
                    = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceSubgroupProperties>();
                if (!vku::contains(properties2.get<vk::PhysicalDeviceSubgroupProperties>().supportedOperations, vk::SubgroupFeatureFlagBits::eShuffle)) {
                    // Subgroup shuffle not supported.
                    return 0U;
                }

                return DefaultPhysicalDeviceRater{}(physicalDevice);
            },
            .pNexts = std::tuple{
                vk::PhysicalDeviceHostQueryResetFeatures { vk::True },
                vk::PhysicalDeviceDescriptorIndexingFeatures{}
                    .setDescriptorBindingStorageImageUpdateAfterBind(vk::True)
                    .setRuntimeDescriptorArray(vk::True),
            },
        } };
    }

    [[nodiscard]] auto createAllocator() const -> vku::Allocator {
        return { vma::AllocatorCreateInfo {
            {},
            *physicalDevice, *device,
            {}, {}, {}, {}, {},
            *instance,
            vk::makeApiVersion(0, 1, 2, 0),
        } };
    }

    [[nodiscard]] auto createDescriptorPool() const -> vk::raii::DescriptorPool {
        constexpr std::array poolSizes {
            vk::DescriptorPoolSize { vk::DescriptorType::eStorageImage, 32 },
        };
        return { device, vk::DescriptorPoolCreateInfo {
            vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            2,
            poolSizes,
        } };
    }

    [[nodiscard]] auto createCommandPool(
        std::uint32_t queueFamilyIndex
    ) const -> vk::raii::CommandPool {
        return { device, vk::CommandPoolCreateInfo {
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueFamilyIndex,
        } };
    }

    [[nodiscard]] static auto createInstance() -> Instance {
        return Instance { vk::ApplicationInfo {
            "mipmap", 0,
            {}, 0,
            vk::makeApiVersion(0, 1, 2, 0),
        } };
    }
};

int main(int argc, char **argv) {
    if (argc != 3) {
        std::println(std::cerr, "Usage: {} <image-path> <output-dir>", argv[0]);
        std::terminate();
    }

    MainApp{}.run(argv[1], argv[2]);
}