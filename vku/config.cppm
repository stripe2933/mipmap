module;

#include <vulkan/vulkan_hpp_macros.hpp>

export module vku:config;

import std;
export import vulkan_hpp;
import :details;

namespace vku {
    export class Instance {
    public:
        template <details::tuple_like PNextsTuple = std::tuple<>>
        struct Config {
            std::vector<const char*> instanceLayers;
            std::vector<const char*> instanceExtensions;
            PNextsTuple pNexts;
        };

        vk::raii::Context context;
        vk::raii::Instance instance;

        template <typename... ConfigArgs>
        explicit Instance(
            const vk::ApplicationInfo &applicationInfo,
            Config<ConfigArgs...> config = {}
        ) : instance { createInstance(applicationInfo, config) } {
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
#endif
        }

    private:
        template <typename... ConfigArgs>
        [[nodiscard]] auto createInstance(
            const vk::ApplicationInfo &applicationInfo,
            Config<ConfigArgs...> &config
        ) const -> vk::raii::Instance {
#ifndef NDEBUG
            config.instanceLayers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif
#if __APPLE__
            config.instanceExtensions.append_range(std::array {
#if VKU_VK_VERSION < 1001000
                vk::KHRGetPhysicalDeviceProperties2ExtensionName,
#endif
                vk::KHRPortabilityEnumerationExtensionName,
            });
#endif

            return { context, std::apply([&](const auto &...pNexts) {
                return vk::StructureChain { vk::InstanceCreateInfo {
#if __APPLE__
                    vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
#else
                    {},
#endif
                    &applicationInfo,
                    config.instanceLayers,
                    config.instanceExtensions,
                }, pNexts... };
            }, config.pNexts).get() };
        }
    };


    export template <typename QueueFamilyIndices, typename Queues>
        requires std::constructible_from<Queues, vk::Device, const QueueFamilyIndices&>
            && requires(const QueueFamilyIndices &queueFamilyIndices) {
                { Queues::getDeviceQueueCreateInfos(queueFamilyIndices) } -> std::ranges::contiguous_range;
            }
    class Device {
    public:
        struct DefaultQueueFamilyIndicesGetter {
            [[nodiscard]] auto operator()(
                vk::PhysicalDevice physicalDevice
            ) const -> QueueFamilyIndices {
                return QueueFamilyIndices { physicalDevice };
            }
        };

        struct DefaultPhysicalDeviceRater {
            std::function<QueueFamilyIndices(vk::PhysicalDevice)> queueFamilyIndicesGetter = DefaultQueueFamilyIndicesGetter{};

            [[nodiscard]] auto operator()(
                vk::PhysicalDevice physicalDevice
            ) const -> std::uint32_t {
                // Check if given physical device has required queue families.
                try {
                    const QueueFamilyIndices queueFamilyIndices = queueFamilyIndicesGetter(physicalDevice);
                }
                catch (...) {
                    return 0U;
                }

                // Rate physical device based on its properties.
                std::uint32_t score = 0;
                const vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
                if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                    score += 1000;
                }
                score += properties.limits.maxImageDimension2D;
                return score;
            }
        };

        template <details::tuple_like PNextsTuple = std::tuple<>>
        struct Config {
            std::function<QueueFamilyIndices(vk::PhysicalDevice)> queueFamilyIndicesGetter = DefaultQueueFamilyIndicesGetter{};
            std::function<std::uint32_t(vk::PhysicalDevice)> physicalDeviceRater = DefaultPhysicalDeviceRater { queueFamilyIndicesGetter };
            std::vector<const char*> deviceExtensions;
            std::optional<vk::PhysicalDeviceFeatures> physicalDeviceFeatures;
            PNextsTuple pNexts;
        };

        vk::raii::PhysicalDevice physicalDevice;
        QueueFamilyIndices queueFamilyIndices;
        vk::raii::Device device;
        Queues queues { *device, queueFamilyIndices };

        template <typename... ConfigArgs>
        explicit Device(
            const vk::raii::Instance &instance,
            Config<ConfigArgs...> config = {}
        ) : physicalDevice { selectPhysicalDevice(instance, config) },
            queueFamilyIndices { std::invoke(config.queueFamilyIndicesGetter, *physicalDevice) },
            device { createDevice(config) } {
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
#endif
        }

        template <typename... ConfigArgs>
        [[nodiscard]] auto selectPhysicalDevice(
            const vk::raii::Instance &instance,
            const Config<ConfigArgs...> &config
        ) const -> vk::raii::PhysicalDevice {
            const std::vector physicalDevices = instance.enumeratePhysicalDevices();
            vk::raii::PhysicalDevice bestPhysicalDevice
                = *std::ranges::max_element(physicalDevices, {}, [&](const vk::raii::PhysicalDevice &physicalDevice) {
                    return std::invoke(config.physicalDeviceRater, *physicalDevice);
                });
            if (std::invoke(config.physicalDeviceRater, *bestPhysicalDevice) == 0U) {
                throw std::runtime_error { "No adequate physical device" };
            }

            return bestPhysicalDevice;
        }

        template <typename... ConfigArgs>
        [[nodiscard]] auto createDevice(
            Config<ConfigArgs...> &config
        ) const -> vk::raii::Device {
#if __APPLE__
            config.deviceExtensions.push_back(vk::KHRPortabilitySubsetExtensionName);
#endif

            const auto queueCreateInfos = Queues::getDeviceQueueCreateInfos(queueFamilyIndices);
            return { physicalDevice, std::apply([&](const auto &...args) {
                return vk::StructureChain { vk::DeviceCreateInfo {
                    {},
                    queueCreateInfos,
                    {},
                    config.deviceExtensions,
                    config.physicalDeviceFeatures ? &*config.physicalDeviceFeatures : nullptr,
                }, args... };
            }, config.pNexts).get() };
        }
    };
}