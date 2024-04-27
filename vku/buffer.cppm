export module vku:buffer;

import std;
export import vulkan_hpp;
export import :allocator;

#define FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

namespace vku {
    export struct Buffer {
        vk::Buffer buffer;
        vk::DeviceSize size;

        constexpr operator vk::Buffer() const noexcept {
            return buffer;
        }
    };

    export struct AllocatedBuffer : Buffer {
        vma::Allocator allocator;
        vma::Allocation allocation;

        AllocatedBuffer(
            vma::Allocator _allocator,
            const vk::BufferCreateInfo &createInfo,
            const vma::AllocationCreateInfo &allocationCreateInfo
        ) : Buffer { nullptr, createInfo.size },
            allocator { _allocator } {
            std::tie(buffer, allocation) = allocator.createBuffer(createInfo, allocationCreateInfo);
        }
        AllocatedBuffer(const AllocatedBuffer&) = delete;
        AllocatedBuffer(AllocatedBuffer &&src) noexcept
            : Buffer { static_cast<Buffer>(src) },
              allocator { src.allocator },
              allocation { std::exchange(src.allocation, nullptr) } { }
        auto operator=(const AllocatedBuffer&) -> AllocatedBuffer& = delete;
        auto operator=(AllocatedBuffer &&src) noexcept -> AllocatedBuffer& {
            if (allocation) {
                allocator.destroyBuffer(buffer, allocation);
            }

            static_cast<Buffer&>(*this) = static_cast<Buffer>(src);
            allocator = src.allocator;
            buffer = std::exchange(src.buffer, nullptr);
            return *this;
        }
        ~AllocatedBuffer() {
            if (allocation) {
                allocator.destroyBuffer(buffer, allocation);
            }
        }

        constexpr operator vk::Buffer() const noexcept {
            return buffer;
        }
    };

    export struct MappedBuffer : AllocatedBuffer {
        void *data;

        MappedBuffer(
            AllocatedBuffer buffer
        ) : AllocatedBuffer { std::move(buffer) },
            data { allocator.mapMemory(allocation) } { }
        MappedBuffer(const MappedBuffer&) = delete;
        MappedBuffer(MappedBuffer &&src) noexcept = default;
        auto operator=(const MappedBuffer&) -> MappedBuffer& = delete;
        auto operator=(MappedBuffer &&src) noexcept -> MappedBuffer& {
            if (allocation) {
                allocator.unmapMemory(allocation);
            }
            static_cast<AllocatedBuffer&>(*this) = static_cast<AllocatedBuffer&&>(std::move(src));
            data = src.data;
            return *this;
        }
        ~MappedBuffer() {
            if (allocation) {
                allocator.unmapMemory(allocation);
            }
        }

        template <typename R>
            requires std::ranges::input_range<R> && std::ranges::sized_range<R>
        static auto fromRange(
            vma::Allocator allocator,
            R &&r,
            vk::BufferUsageFlags usage,
            const vma::AllocationCreateInfo &allocationCreateInfo = {
                vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                vma::MemoryUsage::eAuto,
            }
        ) -> MappedBuffer {
            MappedBuffer result = AllocatedBuffer { allocator, vk::BufferCreateInfo {
                {},
                r.size() * sizeof(std::ranges::range_value_t<R>),
                usage,
            }, allocationCreateInfo };
            std::ranges::copy(FWD(r), static_cast<std::ranges::range_value_t<R>*>(result.data));
            return result;
        }

        template <typename T>
        static auto fromValue(
            vma::Allocator allocator,
            const T &value,
            vk::BufferUsageFlags usage,
            const vma::AllocationCreateInfo &allocationCreateInfo = {
                vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                vma::MemoryUsage::eAuto,
            }
        ) -> MappedBuffer {
            MappedBuffer result = AllocatedBuffer { allocator, vk::BufferCreateInfo {
                {},
                sizeof(T),
                usage,
            }, allocationCreateInfo };
            *static_cast<T*>(result.data) = value;
            return result;
        }
    };
}