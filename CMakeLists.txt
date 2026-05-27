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
        GIT_TAG v4.3.2 # Or use a recent stable release like v4.4.2
)
# Disable KTX command-line applications and tests to keep build lightweight
set(KTX_FEATURE_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_SAMPLER OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ktx)

find_package(Vulkan REQUIRED)

add_executable(RenderEngine
        src/main.cpp
        src/Window.cpp
        src/Window.h
        src/Application.cpp
        src/Application.h
        src/Device.cpp
        src/Device.h
        src/Buffer.cpp
        src/Buffer.h
        src/ResourceHeap.cpp
        src/ResourceHeap.h
        src/Texture.cpp
        src/Texture.h
)

target_link_libraries(RenderEngine
        glfw
        Vulkan::Vulkan
        ktx
)