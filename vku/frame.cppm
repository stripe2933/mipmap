export module vku:frame;

import std;
export import vulkan_hpp;

namespace vku{
    export class InFlightFrame{
    public:
        explicit InFlightFrame(
            const vk::raii::Device &device
        ) : device { device } { }

        auto wait(
            std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()
        ) const -> void {
            if (device.waitForFences(*fence, true, timeout) != vk::Result::eSuccess) {
                throw std::runtime_error { "Failed to wait for in-flight fence" };
            }
        }

        auto reset() const -> void {
            device.resetFences(*fence);
        }

    protected:
        const vk::raii::Device &device;

        vk::raii::Fence fence { device, vk::FenceCreateInfo { vk::FenceCreateFlagBits::eSignaled } };
    };
};