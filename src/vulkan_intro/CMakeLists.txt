# Step 1
add_executable(melkior_step1
    step1.cpp
)

target_link_libraries(melkior_step1 PRIVATE Vulkan::Vulkan)

# Step 2
add_executable(melkior_step2
    step2.cpp
)

target_link_libraries(melkior_step2 PRIVATE Vulkan::Vulkan melkior_engine_lib)

# Step 3
add_executable(melkior_step3
    step3.cpp
)

target_link_libraries(melkior_step3 PRIVATE Vulkan::Vulkan melkior_engine_lib)