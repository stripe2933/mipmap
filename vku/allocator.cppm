export module vku:allocator;

import std;
export import vk_mem_alloc_hpp;
export import vulkan_hpp;

namespace vku {
    export struct Allocator : vma::Allocator {
        Allocator(
            const vma::AllocatorCreateInfo &createInfo
        ) : vma::Allocator { createAllocator(createInfo) } { }
        Allocator(const Allocator&) = delete;
        Allocator(Allocator &&src) = delete;
        ~Allocator() {
            destroy();
        }
    };
}