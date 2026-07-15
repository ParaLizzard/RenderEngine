cmake_minimum_required(VERSION 3.20)
project(RenderEngine)

set(CMAKE_CXX_STANDARD 20)

# ---------------------------------------------------------------------------
# Debug symbols for profiling (Nsight, etc.)
# Keep debug info even in Release builds for accurate GPU profiling
# ---------------------------------------------------------------------------
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG:FULL")
else()
    # MinGW: -g = DWARF debug symbols, -O3 = max optimization
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O1 -g")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3 -g")
endif()

include(FetchContent)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.3.2
)
set(KTX_FEATURE_APPS           OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS          OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_SAMPLER OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON  CACHE BOOL "" FORCE)

FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG        v0.8.0
)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
)

FetchContent_MakeAvailable(glfw ktx fastgltf stb)

# stb is header-only — expose it as an INTERFACE target so we can link it cleanly
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${stb_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Vulkan + glslc
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED COMPONENTS glslc)

if(Vulkan_GLSLC_EXECUTABLE)
    set(GLSLC "${Vulkan_GLSLC_EXECUTABLE}")
else()
    find_program(GLSLC glslc REQUIRED
            HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin"
            DOC "glslc shader compiler")
endif()
message(STATUS "glslc: ${GLSLC}")

# ---------------------------------------------------------------------------
# Shader compilation — only recompile on source or #include changes
# ---------------------------------------------------------------------------
set(SHADER_STAGE_EXTENSIONS
        vert frag comp geom
        tesc tese
        mesh task
        rgen rchit rmiss rint rahit rcall
)

set(SHADER_SOURCES "")
foreach(EXT ${SHADER_STAGE_EXTENSIONS})
    # CONFIGURE_DEPENDS ensures CMake re-evaluates glob when new shaders are added
    file(GLOB_RECURSE _found LIST_DIRECTORIES false CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.${EXT}")
    list(APPEND SHADER_SOURCES ${_found})
endforeach()

set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SHADER_OUTPUTS "")

foreach(SHADER_SRC ${SHADER_SOURCES})
    file(RELATIVE_PATH REL_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${SHADER_SRC}")

    set(SPV_OUT   "${SHADER_BINARY_DIR}/${REL_PATH}.spv")
    set(DEP_FILE  "${SHADER_BINARY_DIR}/${REL_PATH}.d")

    get_filename_component(SPV_DIR   "${SPV_OUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${SPV_DIR}")


    file(RELATIVE_PATH SRC_REL "${CMAKE_BINARY_DIR}" "${SHADER_SRC}")
    file(RELATIVE_PATH INCLUDE_REL "${CMAKE_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(OUT_REL "shaders/${REL_PATH}.spv")
    set(DEP_REL "shaders/${REL_PATH}.d")

    add_custom_command(
            OUTPUT  "${SPV_OUT}"
            COMMAND "${GLSLC}"
            -g
            -O
            -MD -MF "${DEP_REL}"
            -I "${INCLUDE_REL}"
            --target-env=vulkan1.3
            "${SRC_REL}"
            -o "${OUT_REL}"
            DEPENDS "${SHADER_SRC}"
            DEPFILE "${DEP_FILE}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Compiling shader ${REL_PATH} -> ${REL_PATH}.spv"
            VERBATIM
    )

    list(APPEND SHADER_OUTPUTS "${SPV_OUT}")
endforeach()

# ---------------------------------------------------------------------------
# Engine executable
# ---------------------------------------------------------------------------
set(CORE_SOURCES
        src/Core/main.cpp
        src/Core/Application.cpp
        src/Core/Application.h
        src/Core/EngineConfig.h
        src/Core/Exception.cpp
        src/Core/Exception.h
)

set(SYSTEM_SOURCES
        src/System/Window/Window.cpp
        src/System/Window/Window.h
        src/System/Window/WindowInterface.h
        src/System/Window/WindowWin32.cpp
        src/System/Window/WindowWin32.h
        src/System/Input/InputManager.cpp
        src/System/Input/InputManager.h
        src/System/Input/InputBackend.h
        src/System/Input/InputBackendWindows.cpp
        src/System/Input/InputBackendWindows.h
        src/System/Input/InputTypes.h
)

set(THREADING_SOURCES
        src/Threading/JobSystem.h
)

set(VULKAN_SOURCES
        src/Vulkan/Device.cpp
        src/Vulkan/Device.h
        src/Vulkan/Buffer.cpp
        src/Vulkan/Buffer.h
        src/Vulkan/Descriptor.cpp
        src/Vulkan/Descriptor.h
        src/Vulkan/VkUtils.h
        src/Vulkan/Swapchain.cpp
        src/Vulkan/Swapchain.h
        src/Vulkan/ResourceHeap.cpp
        src/Vulkan/ResourceHeap.h
)

set(RENDERER_SOURCES
        src/Renderer/Renderer.cpp
        src/Renderer/Renderer.h
        src/Renderer/RenderGraph.cpp
        src/Renderer/RenderGraph.h
        src/Renderer/RenderPassNode.h
        src/Renderer/ShaderUtils.h
        src/Renderer/FrameInfo.h
)

set(PASSES_SOURCES
        src/Renderer/Passes/FxaaPassNode.h
        src/Renderer/Passes/FxaaPassNode.cpp
        src/Renderer/Passes/SsaoPassNode.cpp
        src/Renderer/Passes/SsaoPassNode.h
        src/Renderer/Passes/VisibilityPassNode.cpp
        src/Renderer/Passes/VisibilityPassNode.h
        src/Renderer/Passes/CullPassNode.cpp
        src/Renderer/Passes/CullPassNode.h
        src/Renderer/Passes/MaterialPassNode.cpp
        src/Renderer/Passes/MaterialPassNode.h
)

set(SCENE_SOURCES
        src/Scene/SceneManager.cpp
        src/Scene/SceneManager.h
        src/Scene/GameObject.cpp
        src/Scene/GameObject.h
        src/Scene/Camera.cpp
        src/Scene/Camera.h
        src/Scene/KeyboardMovement.cpp
        src/Scene/KeyboardMovement.h
)

set(ASSETSYSTEM_SOURCES
        src/AssetSystem/AssetStreamer.cpp
        src/AssetSystem/AssetStreamer.h
        src/AssetSystem/Texture.cpp
        src/AssetSystem/Texture.h
        src/AssetSystem/Model.h
        src/AssetSystem/Model.cpp
        src/AssetSystem/LoaderGLTF.cpp
        src/AssetSystem/LoaderGLTF.h
        src/AssetSystem/IBL.cpp
        src/AssetSystem/IBL.h
)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/models" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(RenderEngine
        ${CORE_SOURCES}
        ${SYSTEM_SOURCES}
        ${THREADING_SOURCES}
        ${VULKAN_SOURCES}
        ${RENDERER_SOURCES}
        ${SCENE_SOURCES}
        ${ASSETSYSTEM_SOURCES}
        ${PASSES_SOURCES}
)

target_include_directories(RenderEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(RenderEngine
        glfw
        Vulkan::Vulkan
        ktx
        fastgltf
        stb
)


target_sources(RenderEngine PRIVATE ${SHADER_OUTPUTS})