cmake_minimum_required(VERSION 3.16)
project(vulkan_compute LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

add_executable(vulkan_compute
    src/main.cpp
)

target_link_libraries(vulkan_compute Vulkan::Vulkan)

add_custom_target(shaders
    COMMAND glslc ${CMAKE_SOURCE_DIR}/shaders/compute.comp
            -o ${CMAKE_BINARY_DIR}/compute.spv
)

add_dependencies(vulkan_compute shaders)
