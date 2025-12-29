message("Currently here: ${CMAKE_CURRENT_SOURCE_DIR} ")

add_library(melkior_engine_lib
    src/engine.cpp
)

target_include_directories(melkior_engine_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(melkior_engine_lib PUBLIC Vulkan::Vulkan)
