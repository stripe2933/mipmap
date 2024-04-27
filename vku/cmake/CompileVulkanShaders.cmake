# Compile shader files and copy them into build directory.
#
# Usage:
# compile_shaders(target
#     path1/shader1.vert
#     path2/shader2.frag
#     ...
# )
#
# Result: path1/shader1.vert.spv, path2/shader2.frag.spv, ... in build directory.

find_package(Vulkan COMPONENTS glslc REQUIRED)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

# Set optimization flag based on build type.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(GLSLC_OPTIMIZATION_FLAG "-O0")
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(GLSLC_OPTIMIZATION_FLAG "-Os")
else()
    set(GLSLC_OPTIMIZATION_FLAG "-O")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    file(DOWNLOAD https://raw.githubusercontent.com/MiSo1289/cmake-embed/master/cmake/EmbedResources.cmake
        ${CMAKE_BINARY_DIR}/EmbedResources.cmake
    )
    include(${CMAKE_BINARY_DIR}/EmbedResources.cmake)
endif()

function(compile_shaders target)
    # Get the rest of the arguments after the target name
    set(sources ${ARGN})

    foreach(source ${sources})
        set(SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/${source}.spv)
        string(MAKE_C_IDENTIFIER ${source} SHADER_IDENTIFIER)

        # Extract 1st and 4th digits
        string(SUBSTRING ${VKU_VK_VERSION} 0 1 FIRST_DIGIT)
        string(SUBSTRING ${VKU_VK_VERSION} 3 1 FOURTH_DIGIT)

        # Combine them with a period delimiter
        set(TARGET_ENV_VK_VERSION "--target-env=vulkan${FIRST_DIGIT}.${FOURTH_DIGIT}")

        add_custom_command(
            OUTPUT ${SHADER_OUTPUT_PATH}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
            COMMAND ${glslc_executable}
                ${TARGET_ENV_VK_VERSION}
                ${GLSLC_OPTIMIZATION_FLAG}
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
            -o ${SHADER_OUTPUT_PATH}
        )

        list(APPEND SHADER_OUTPUT_PATHS ${SHADER_OUTPUT_PATH})
        list(APPEND SHADER_IDENTIFIERS ${SHADER_IDENTIFIER})
    endforeach()

    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        add_embedded_binary_resources(
            ${target}-shaders
            OUT_DIR resources
            HEADER shaders.hpp
            NAMESPACE resources
            RESOURCE_NAMES ${SHADER_IDENTIFIERS}
            RESOURCES ${SHADER_OUTPUT_PATHS}
        )
        target_link_libraries(${target} PRIVATE ${target}-shaders)
    else()
        target_sources(${target} PRIVATE ${SHADER_OUTPUT_PATHS})
    endif()
endfunction()