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

find_package(Vulkan REQUIRED COMPONENTS glslc)

# ---------------------------------------------------------------------------
# Shader compilation
# ---------------------------------------------------------------------------
# Locate glslc — prefer the one bundled with the Vulkan SDK so the version
# matches the SDK headers already in use.
if(Vulkan_GLSLC_EXECUTABLE)
    set(GLSLC "${Vulkan_GLSLC_EXECUTABLE}")
else()
    find_program(GLSLC glslc REQUIRED
            HINTS
            "$ENV{VULKAN_SDK}/bin"
            "$ENV{VULKAN_SDK}/Bin"
            DOC "glslc shader compiler")
endif()

message(STATUS "glslc: ${GLSLC}")

# All GLSL stage extensions we want to compile.
set(SHADER_STAGE_EXTENSIONS
        vert frag comp geom
        tesc tese
        mesh task
        rgen rchit rmiss rint rahit rcall
)

# Collect every shader source under root/shaders/
set(SHADER_SOURCES "")
foreach(EXT ${SHADER_STAGE_EXTENSIONS})
    file(GLOB_RECURSE _found
            LIST_DIRECTORIES false
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.${EXT}"
    )
    list(APPEND SHADER_SOURCES ${_found})
endforeach()

# Output SPVs mirror the source tree under <build>/shaders/
set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SHADER_OUTPUTS "")

foreach(SHADER_SRC ${SHADER_SOURCES})
    # e.g. <root>/shaders/forward/pbr.frag  ->  forward/pbr.frag
    file(RELATIVE_PATH REL_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${SHADER_SRC}"
    )

    set(SPV_OUT "${SHADER_BINARY_DIR}/${REL_PATH}.spv")
    set(DEP_FILE "${SHADER_BINARY_DIR}/${REL_PATH}.d")

    # Make sure the output subdirectory exists at generation time.
    get_filename_component(SPV_DIR "${SPV_OUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${SPV_DIR}")

    add_custom_command(
            OUTPUT  "${SPV_OUT}"
            COMMAND "${GLSLC}"
            # Emit a Make-style dependency file so CMake can track #includes.
            -MD -MF "${DEP_FILE}"
            # Pass the shaders/ root as an include search path.
            -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            # Target Vulkan 1.3 SPIR-V 1.6 — adjust if needed.
            --target-env=vulkan1.3
            -O
            "${SHADER_SRC}"
            -o "${SPV_OUT}"
            DEPENDS "${SHADER_SRC}"
            DEPFILE "${DEP_FILE}"
            COMMENT "glslc ${REL_PATH} -> ${REL_PATH}.spv"
            VERBATIM
    )

    list(APPEND SHADER_OUTPUTS "${SPV_OUT}")
endforeach()

# A single logical target that drives all shader compilations.
add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})
# ---------------------------------------------------------------------------

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
        src/Scene/Model.h
        src/Scene/Model.cpp
        src/Scene/GameObject.cpp
        src/Scene/GameObject.h
        src/Scene/Camera.cpp
        src/Scene/LoaderGLTF.cpp
        src/Scene/LoaderGLTF.h
)

set(PASSES_SOURCES
        src/Passes/ForwardPassNode.cpp
        src/Passes/ForwardPassNode.h
)

add_executable(RenderEngine
        ${CORE_SOURCES}
        ${RENDERER_SOURCES}
        ${SCENE_SOURCES}
        ${PASSES_SOURCES}
)

target_include_directories(RenderEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RenderEngine
        glfw
        Vulkan::Vulkan
        ktx
)

add_dependencies(RenderEngine Shaders)