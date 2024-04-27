cmake_minimum_required(VERSION 3.29)
project(mipmap)

set(CMAKE_CXX_STANDARD 23)

# --------------------
# Enable standard library module.
# --------------------

file(DOWNLOAD https://raw.githubusercontent.com/stripe2933/CppStandardLibraryModule/main/cmake/EnableStandardLibraryModule.cmake
    ${PROJECT_BINARY_DIR}/EnableStandardLibraryModule.cmake
)
include(${PROJECT_BINARY_DIR}/EnableStandardLibraryModule.cmake)

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

# ----------------
# Internal dependencies.
# ----------------

set(VKU_VK_VERSION 1002000)
add_subdirectory(vku)

# ----------------
# Project executables.
# ----------------

add_executable(mipmap main.cpp impl.cpp)
target_sources(mipmap PRIVATE
    FILE_SET CXX_MODULES
    FILES
        extlibs/image_data.cppm
        extlibs/missing_std.cppm
)
target_link_libraries(mipmap PRIVATE vku)

# ----------------
# Shader compilations.
# ----------------

compile_shaders(mipmap
    shaders/mipmap.comp
    shaders/subgroup_mipmap.comp
)
