cmake_minimum_required(VERSION 3.20)
project(RenderEngine)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG v4.3.2
)

set(KTX_FEATURE_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_SAMPLER OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ktx)

find_package(Vulkan REQUIRED)

# Core sources
set(CORE_SOURCES
        src/Core/main.cpp
        src/Core/Window.cpp
        src/Core/Application.cpp
        src/Core/Device.cpp
        src/Core/Buffer.cpp
)

# Renderer sources
set(RENDERER_SOURCES
        src/Renderer/Renderer.cpp
        src/Renderer/Swapchain.cpp
        src/Renderer/RenderGraph.cpp
        src/Renderer/ResourceHeap.cpp
)

# Scene sources
set(SCENE_SOURCES
        src/Scene/Texture.cpp
)

add_executable(RenderEngine
        ${CORE_SOURCES}
        ${RENDERER_SOURCES}
        ${SCENE_SOURCES}
)

# Add include directories (from project root)
# Now you can use: #include "Core/Window.h" instead of #include "../Core/Window.h"
target_include_directories(RenderEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RenderEngine
        glfw
        Vulkan::Vulkan
        ktx
)