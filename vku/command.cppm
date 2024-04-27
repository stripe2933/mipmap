export module vku:command;

import std;
export import vulkan_hpp;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace vku {
namespace details {
    template <typename T, typename R, typename... Args>
    concept invocable_r = std::invocable<T, Args...> && std::convertible_to<std::invoke_result_t<T, Args...>, R>;
}

    export auto executeSingleCommand(
        vk::Device device,
        vk::CommandPool commandPool,
        vk::Queue queue,
        details::invocable_r<void, vk::CommandBuffer> auto &&f,
        vk::Fence fence = {}
    ) -> void {
        const vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            1,
        })[0];
        commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        std::invoke(FWD(f), commandBuffer);
        commandBuffer.end();
        queue.submit(vk::SubmitInfo {
            {},
            {},
            commandBuffer,
        }, fence);
    }

    export template <std::invocable<vk::CommandBuffer> F>
    auto executeSingleCommand(
        vk::Device device,
        vk::CommandPool commandPool,
        vk::Queue queue,
        F &&f,
        vk::Fence fence = {}
    ) -> std::invoke_result_t<F> {
        const vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            1,
        })[0];
        commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        auto result = std::invoke(FWD(f), commandBuffer);
        commandBuffer.end();
        queue.submit(vk::SubmitInfo {
            {},
            {},
            commandBuffer,
        }, fence);
        return result;
    }
}