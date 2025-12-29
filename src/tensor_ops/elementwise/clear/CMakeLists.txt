add_executable(melkior_clear
    main.cpp
)

target_link_libraries(melkior_clear Vulkan::Vulkan)

add_custom_target(melkior_clear_shaders
    COMMAND glslc ${CMAKE_SOURCE_DIR}/src/tensor_ops/elementwise/clear/shaders/clear.comp
            -o ${CMAKE_BINARY_DIR}/bin/clear.spv
)

add_dependencies(melkior_clear melkior_clear_shaders)
