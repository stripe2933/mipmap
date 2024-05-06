module;

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

export module vku:imgui;

import std;
export import vulkan_hpp;

namespace vku{
    export class ImGuiRenderer {
    public:
        static auto init(
            GLFWwindow *window,
            vk::Device device,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Queue graphicsQueue,
            vk::DescriptorPool descriptorPool,
            std::uint32_t frameCount,
            vk::Format colorAttachmentFormat
        ) -> void {
            ImGuiIO &io                = ImGui::GetIO();
            io.DisplaySize             = getFramebufferSize(window);
            io.FontGlobalScale         = 1.0f;
            io.DisplayFramebufferScale = getContentScale(window);
            ImGui_ImplGlfw_InitForVulkan(window, true);

            ImGui_ImplVulkan_InitInfo initInfo {
                .Instance = instance,
                .PhysicalDevice = physicalDevice,
                .Device = device,
                .Queue = graphicsQueue,
                .DescriptorPool = descriptorPool,
                .MinImageCount = frameCount,
                .ImageCount = frameCount,
                .UseDynamicRendering = true,
                .ColorAttachmentFormat = static_cast<VkFormat>(colorAttachmentFormat),
            };
            ImGui_ImplVulkan_Init(&initInfo, nullptr);
        }

        static auto init(
            GLFWwindow *window,
            vk::Device device,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Queue graphicsQueue,
            vk::DescriptorPool descriptorPool,
            std::uint32_t frameCount,
            vk::RenderPass renderPass,
            std::uint32_t subpass
        ) -> void {
            ImGuiIO &io                = ImGui::GetIO();
            io.DisplaySize             = getFramebufferSize(window);
            io.FontGlobalScale         = 1.0f;
            io.DisplayFramebufferScale = getContentScale(window);
            ImGui_ImplGlfw_InitForVulkan(window, true);

            ImGui_ImplVulkan_InitInfo initInfo {
                .Instance = instance,
                .PhysicalDevice = physicalDevice,
                .Device = device,
                .Queue = graphicsQueue,
                .DescriptorPool = descriptorPool,
                .Subpass = subpass,
                .MinImageCount = frameCount,
                .ImageCount = frameCount,
            };
            ImGui_ImplVulkan_Init(&initInfo, renderPass);
        }

        static auto shutdown() -> void {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
        }

        static auto newFrame() -> void {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        static auto draw(
            vk::CommandBuffer commandBuffer
        ) -> void {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        }

        [[nodiscard]] static auto createDescriptorPool(
            const vk::raii::Device &device
        ) -> vk::raii::DescriptorPool {
            constexpr std::array poolSizes {
                vk::DescriptorPoolSize { vk::DescriptorType::eCombinedImageSampler, 512 },
            };
            const vk::DescriptorPoolCreateInfo poolInfo {
                vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                128,
                poolSizes,
            };
            return { device, poolInfo };
        }

    private:
        [[nodiscard]] static auto getFramebufferSize(
            GLFWwindow *window
        ) -> ImVec2 {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            return { static_cast<float>(width), static_cast<float>(height) };
        }

        [[nodiscard]] static auto getContentScale(
            GLFWwindow *window
        ) -> ImVec2 {
            ImVec2 result;
            glfwGetWindowContentScale(window, &result.x, &result.y);
            return result;
        }
    };
}
