export module vku:allocator;

import std;
export import vk_mem_alloc_hpp;
export import vulkan_hpp;

namespace vku {
    export struct Allocator {
        vma::Allocator allocator;

        Allocator(
            const vma::AllocatorCreateInfo &createInfo
        ) : allocator { createAllocator(createInfo) } { }
        Allocator(const Allocator&) = delete;
        Allocator(Allocator &&src) noexcept : allocator { std::exchange(src.allocator, nullptr) } { }
        ~Allocator() {
            allocator.destroy();
        }

        [[nodiscard]] constexpr operator vma::Allocator() const noexcept {
            return allocator;
        }
    };
}