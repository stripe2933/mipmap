#include <iostream>
#include <print>
#include <set>

#include <ImageData.hpp>
#include <ranges.hpp>
#include <stb_image_write.h>
#include <vku/Allocator.hpp>
#include <vku/buffers.hpp>
#include <vku/commands.hpp>
#include <vku/images.hpp>
#include <vku/Instance.hpp>
#include <vku/Gpu.hpp>
#include <vku/utils.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#include "pipelines/MipmapComputer.hpp"
#include "pipelines/SubgroupMipmapComputer.hpp"

#define INDEX_SEQ(Is, N, ...)                          \
    [&]<std::size_t... Is>(std::index_sequence<Is...>) \
        __VA_ARGS__                                    \
    (std::make_index_sequence<N>{})
#define ARRAY_OF(N, ...)                            \
    INDEX_SEQ(Is, N, {                              \
        return std::array { (Is, __VA_ARGS__)... }; \
    })
#define FWD(...) static_cast<decltype(__VA_ARGS__) &&>(__VA_ARGS__)

struct QueueFamilyIndices {
    std::uint32_t computeGraphics;

    explicit QueueFamilyIndices(
        vk::PhysicalDevice physicalDevice
    ) {
        for (auto [queueFamilyIndex, properties] : physicalDevice.getQueueFamilyProperties() | ranges::views::enumerate) {
            if (properties.queueFlags & vk::QueueFlagBits::eCompute && properties.queueFlags & vk::QueueFlagBits::eGraphics) {
                computeGraphics = queueFamilyIndex;
                return;
            }
        }

        throw std::runtime_error { "Physical device doesn't have compute-graphics queue family" };
    }
};

struct Queues {
    vk::Queue computeGraphics;

    Queues(
        vk::Device device,
        const QueueFamilyIndices &queueFamilyIndices
    ) : computeGraphics { device.getQueue(queueFamilyIndices.computeGraphics, 0) } { }

    [[nodiscard]] static auto getDeviceQueueCreateInfos(
        const QueueFamilyIndices &queueFamilyIndices
    ) -> std::array<vk::DeviceQueueCreateInfo, 1> {
        static constexpr std::array queuePriorities { 1.f };
        return std::array {
            vk::DeviceQueueCreateInfo { {}, queueFamilyIndices.computeGraphics, queuePriorities },
        };
    }
};

class MainApp : vku::Instance, vku::Gpu<QueueFamilyIndices, Queues> {
public:
    MainApp()
        : Instance { createInstance() },
          Gpu { createGpu() } { }

    auto run(
        const std::filesystem::path &imagePath,
        const std::filesystem::path &outputDir
    ) const -> void {
        // Load image, calculate the maximum mip levels.
        const ImageData<std::uint8_t> imageData { imagePath.string().c_str(), 4 };
        const vk::Extent2D baseImageExtent { static_cast<std::uint32_t>(imageData.width), static_cast<std::uint32_t>(imageData.height) };
        const std::uint32_t imageMipLevels = vku::Image::maxMipLevels(baseImageExtent);

        // Load image into staging buffer.
        const vku::MappedBuffer imageStagingBuffer {
            allocator,
            std::from_range, imageData.getSpan(),
            vk::BufferUsageFlagBits::eTransferSrc, /* staging src */
        };

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
        vku::executeSingleCommand(*device, *computeGraphicsCommandPool, queues.computeGraphics, [&](vk::CommandBuffer commandBuffer) {
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                {}, {}, {},
                std::apply([](const auto &...images) {
                    return std::array {
                        vk::ImageMemoryBarrier {
                            {}, vk::AccessFlagBits::eTransferWrite,
                            {}, vk::ImageLayout::eTransferDstOptimal,
                            vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                            images,
                            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
                        }...
                    };
                }, baseImages));

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
        queues.computeGraphics.waitIdle();

        // Query pool for timestamp query.
        const vk::raii::QueryPool queryPool { device, vk::QueryPoolCreateInfo {
            {},
            vk::QueryType::eTimestamp,
            2,
        } };
        const auto printElapsedTime = [&queryPool, timestampPeriod = physicalDevice.getProperties().limits.timestampPeriod](std::string_view label) {
            const auto [result, timestamps] = queryPool.getResults<std::uint64_t>(
                0, 2, 2 * sizeof(std::uint64_t), sizeof(std::uint64_t), vk::QueryResultFlagBits::e64);
            if (result == vk::Result::eSuccess) {
                std::println("{}: {} us", label, (timestamps[1] - timestamps[0]) * timestampPeriod / 1e3f);
            }
            else {
                std::println(std::cerr, "Failed to get timestamp query: {}", to_string(result));
            }
        };

        // 1. Blit-based mipmap generation.
        {
            const vku::Image &targetImage = get<0>(baseImages);

            vku::executeSingleCommand(*device, *computeGraphicsCommandPool, queues.computeGraphics, [&](vk::CommandBuffer commandBuffer) {
                commandBuffer.resetQueryPool(*queryPool, 0, 2);
                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

                for (auto [srcLevel, dstLevel] : std::views::iota(0U, targetImage.mipLevels) | ranges::views::pairwise) {
                    commandBuffer.pipelineBarrier(
                        srcLevel == 0U ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                        {}, {}, {},
                        std::array {
                            vk::ImageMemoryBarrier {
                                srcLevel == 0U ? vk::AccessFlagBits::eNone : vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                                vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                                targetImage,
                                { vk::ImageAspectFlagBits::eColor, srcLevel, 1, 0, 1 }
                            },
                            vk::ImageMemoryBarrier {
                                {}, vk::AccessFlagBits::eTransferWrite,
                                {}, vk::ImageLayout::eTransferDstOptimal,
                                vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                                targetImage,
                                { vk::ImageAspectFlagBits::eColor, dstLevel, 1, 0, 1 }
                            },
                        });

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
            queues.computeGraphics.waitIdle();
            printElapsedTime("Blit based mipmap generation");
        }

        // 2. Compute shader mipmap generation with per-level barriers.
        {
            const vku::Image &targetImage = get<1>(baseImages);

            // Prepare the pipeline and descriptor set.
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
                descriptorSets.getDescriptorWrites0(imageMipViews | ranges::views::deref).get(),
                {});

            vku::executeSingleCommand(*device, *computeGraphicsCommandPool, queues.computeGraphics, [&](vk::CommandBuffer commandBuffer) {
                commandBuffer.resetQueryPool(*queryPool, 0, 2);
                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

               commandBuffer.pipelineBarrier(
                   vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader,
                   {}, {}, {},
                   vk::ImageMemoryBarrier {
                       {}, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                       {}, vk::ImageLayout::eGeneral,
                       vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                       targetImage,
                       vku::fullSubresourceRange(),
                   });

                mipmapComputer.compute(commandBuffer, descriptorSets, baseImageExtent, targetImage.mipLevels);

                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
            });
            queues.computeGraphics.waitIdle();
            printElapsedTime("Compute shader mipmap generation with per-level barriers");
        }

        // 3. Compute shader mipmap generation with subgroups.
        {
            const vku::Image &targetImage = get<2>(baseImages);

            // Get subgroup size from physical device properties.
            const std::uint32_t subgroupSize
                = physicalDevice.getProperties2<
                    vk::PhysicalDeviceProperties2,
                    vk::PhysicalDeviceSubgroupProperties>()
                .get<vk::PhysicalDeviceSubgroupProperties>()
                .subgroupSize;

            // Prepare the pipeline and descriptor set.
            const SubgroupMipmapComputer subgroupMipmapComputer { device, targetImage.mipLevels, subgroupSize };
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
                descriptorSets.getDescriptorWrites0(imageMipViews | ranges::views::deref).get(),
                {});

            vku::executeSingleCommand(*device, *computeGraphicsCommandPool, queues.computeGraphics, [&](vk::CommandBuffer commandBuffer) {
                commandBuffer.resetQueryPool(*queryPool, 0, 2);
                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, 0);

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader,
                    {}, {}, {},
                    vk::ImageMemoryBarrier {
                        {}, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                        {}, vk::ImageLayout::eGeneral,
                        vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                        targetImage,
                        vku::fullSubresourceRange(),
                    });

                subgroupMipmapComputer.compute(commandBuffer, descriptorSets, baseImageExtent, targetImage.mipLevels);

                commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 1);
            });
            queues.computeGraphics.waitIdle();
            printElapsedTime("Compute shader mipmap generation with subgroup operation");
        }

        // Create host buffers for destaging.
        const vk::Extent2D destagingImageExtent { baseImageExtent.width * 3U / 2U, baseImageExtent.height };
        const std::array destagingBuffers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
            = ARRAY_OF(3, vku::MappedBuffer { vku::AllocatedBuffer { allocator, vk::BufferCreateInfo {
                {},
                blockSize(vk::Format::eR8G8B8A8Unorm) * destagingImageExtent.width * destagingImageExtent.height,
                vk::BufferUsageFlagBits::eTransferDst /* destaging dst */,
            }, vma::AllocationCreateInfo {
                vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped,
                vma::MemoryUsage::eAuto,
            } } });
#pragma clang diagnostic pop

        // Copy from baseImages[0..3] to destagingBuffers[0..3].
        vku::executeSingleCommand(*device, *computeGraphicsCommandPool, queues.computeGraphics, [&](vk::CommandBuffer commandBuffer) {
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                {}, {}, {},
                std::apply([](const auto &...images) {
                    return std::array {
                        vk::ImageMemoryBarrier {
                            {}, vk::AccessFlagBits::eTransferRead,
                            {}, vk::ImageLayout::eTransferSrcOptimal,
                            vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
                            images,
                            vku::fullSubresourceRange(),
                        }...
                    };
                }, baseImages));

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
        queues.computeGraphics.waitIdle();

        constexpr std::array filenames { "blit.png", "compute_per_level_barriers.png", "compute_subgroup.png" };
        for (const auto &[destagingBuffer, filename] : std::views::zip(destagingBuffers, filenames)) {
            stbi_write_png((outputDir / filename).string().c_str(),
                destagingImageExtent.width, destagingImageExtent.height, 4,
                destagingBuffer.data, blockSize(vk::Format::eR8G8B8A8Unorm) * destagingImageExtent.width);
        }
    }

private:
    vku::Allocator allocator = createAllocator();
    vk::raii::DescriptorPool descriptorPool = createDescriptorPool();
    vk::raii::CommandPool computeGraphicsCommandPool = createCommandPool(queueFamilyIndices.computeGraphics);

    [[nodiscard]] auto createGpu() const -> Gpu {
        return Gpu { instance, Gpu::Config {
            .physicalDeviceRater = [](vk::PhysicalDevice physicalDevice) {
                if (const vk::PhysicalDeviceLimits limits = physicalDevice.getProperties().limits;
                    limits.timestampPeriod == 0.f || !limits.timestampComputeAndGraphics) {
                    // Timestamp query not supported.
                    return 0U;
                }

                const vk::StructureChain properties2
                    = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceSubgroupProperties>();
                if (auto subgroupProperties = properties2.get<vk::PhysicalDeviceSubgroupProperties>(); subgroupProperties.subgroupSize < 8){
                    // Subgroup size must be at lest 8.
                    return 0U;
                }
                else if (!vku::contains(subgroupProperties.supportedOperations, vk::SubgroupFeatureFlagBits::eShuffle)) {
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
            {},
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
        std::exit(1);
    }

    MainApp{}.run(argv[1], argv[2]);
}