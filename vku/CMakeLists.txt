cmake_minimum_required(VERSION 3.28)

project(vku
    LANGUAGES CXX
)

# ----------------
# CMake options.
# ----------------

set(VKU_VK_VERSION 1002000)
option(VKU_USE_GLFW "Use GLFW for window system integration.")
option(VKU_USE_GLM "Use GLM for vector/matrix mathematics. This automatically enabled when VKU_USE_GLFW ON.")
option(VKU_USE_IMGUI "Use ImGui helper.")

if (VKU_USE_GLFW)
    set(VKU_USE_GLM ON)
endif()

# ----------------
# External dependencies.
# ----------------

find_package(Vulkan REQUIRED)
if (VKU_USE_GLM)
    find_package(glm CONFIG REQUIRED)
endif()
if (VKU_USE_GLFW)
    find_package(GLFW3 CONFIG REQUIRED)
endif()
if (VKU_USE_IMGUI)
    find_package(imgui CONFIG REQUIRED)
endif()

CPMAddPackage("gh:stripe2933/VulkanMemoryAllocator-Hpp#master")

# ----------------
# Module configurations for external dependencies.
# ----------------

# glm

if (VKU_USE_GLM)
    file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/glm.cmake
        ${PROJECT_BINARY_DIR}/glm.cmake
    )
    include(${PROJECT_BINARY_DIR}/glm.cmake)

    target_compile_definitions(glm_module PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
    )
endif()

# Vulkan-Hpp

file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/Vulkan.cmake
    ${PROJECT_BINARY_DIR}/Vulkan.cmake
)
include(${PROJECT_BINARY_DIR}/Vulkan.cmake)

if (APPLE)
    target_compile_definitions(VulkanHppModule PUBLIC
        VK_ENABLE_BETA_EXTENSIONS
    )
endif()

# VulkanMemoryAllocator-Hpp

file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/Cpp20Module/main/VulkanMemoryAllocator-Hpp.cmake
    ${PROJECT_BINARY_DIR}/VulkanMemoryAllocator-Hpp.cmake
)
include(${PROJECT_BINARY_DIR}/VulkanMemoryAllocator-Hpp.cmake)

target_compile_definitions(VulkanMemoryAllocator-Hpp_module PUBLIC
    VMA_VULKAN_VERSION=${VKU_VK_VERSION}
)

# ----------------
# CMake scripts.
# ----------------

include(cmake/CompileVulkanShaders.cmake)

# ----------------
# Project libraries.
# ----------------

add_library(vku)
target_compile_features(vku PUBLIC cxx_std_20)
target_sources(vku PUBLIC
    FILE_SET CXX_MODULES
    FILES
        details/concepts.cppm
        details/missing_std.cppm
        details/mod.cppm
        details/ranges.cppm
        details/reflect.cppm
        allocator.cppm
        buffer.cppm
        command.cppm
        config.cppm
        descriptor.cppm
        frame.cppm
        image.cppm
        mod.cppm
        pipeline.cppm
        rendering.cppm
        swapchain.cppm
        utils.cppm
)
target_link_libraries(vku PUBLIC
    VulkanHppModule
    VulkanMemoryAllocator-Hpp::module
)

if (VKU_USE_GLFW)
    target_sources(vku PUBLIC
        FILE_SET CXX_MODULES
        FILES
            window.cppm
    )
    target_link_libraries(vku PUBLIC
        glfw
    )
    target_compile_definitions(vku PUBLIC
        VKU_USE_GLFW
        GLFW_INCLUDE_NONE
    )
endif()
if (VKU_USE_GLM)
    target_link_libraries(vku PUBLIC
        glm::module
    )
    target_compile_definitions(vku PUBLIC
        VKU_USE_GLM
    )
endif()
if (VKU_USE_IMGUI)
    target_link_libraries(vku PUBLIC
        imgui::imgui
    )
    target_sources(vku PUBLIC
        FILE_SET CXX_MODULES
        FILES
            imgui.cppm
    )
    target_compile_definitions(vku PUBLIC
        VKU_USE_IMGUI
    )
endif()