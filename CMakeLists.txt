cmake_minimum_required(VERSION 3.24)
project(mipmap)

# ----------------
# Install CPM.cmake.
# ----------------

file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/cpm.cmake
    ${PROJECT_BINARY_DIR}/CPM.cmake
)
include(${PROJECT_BINARY_DIR}/CPM.cmake)

# ----------------
# External dependencies.
# ----------------

find_package(Stb REQUIRED)

set(VKU_VK_VERSION 1002000)
CPMAddPackage("gh:stripe2933/vku#main")

# ----------------
# Project executables.
# ----------------

add_executable(mipmap main.cpp impl.cpp)
target_include_directories(mipmap PRIVATE extlibs)
target_link_libraries(mipmap PRIVATE vku)

# ----------------
# Shader compilations.
# ----------------

target_compile_shaders(mipmap
    shaders/mipmap.comp
    shaders/subgroup_mipmap_8.comp shaders/subgroup_mipmap_16.comp shaders/subgroup_mipmap_32.comp shaders/subgroup_mipmap_64.comp shaders/subgroup_mipmap_128.comp
)