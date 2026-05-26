cmake_minimum_required(VERSION 3.20)
project(RenderEngine)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Fetch GLFW from GitHub if not found
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
)

target_link_libraries(RenderEngine
        glfw
        Vulkan::Vulkan
)