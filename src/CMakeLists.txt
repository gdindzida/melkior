add_executable(melkior
    main.cpp
)

target_link_libraries(melkior Vulkan::Vulkan)

add_custom_target(shaders
    COMMAND glslc ${CMAKE_SOURCE_DIR}/src/shaders/compute.comp
            -o ${CMAKE_BINARY_DIR}/bin/compute.spv
)

add_dependencies(melkior shaders)

add_subdirectory(engine/)
add_subdirectory(tensor_ops/)
add_subdirectory(vulkan_intro/)
