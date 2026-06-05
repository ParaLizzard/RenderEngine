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
    file(GLOB_RECURSE _found LIST_DIRECTORIES false
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.${EXT}")
    list(APPEND SHADER_SOURCES ${_found})
endforeach()

set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SHADER_STAMP_DIR  "${CMAKE_BINARY_DIR}/shader_stamps")
set(SHADER_OUTPUTS "")

foreach(SHADER_SRC ${SHADER_SOURCES})
    file(RELATIVE_PATH REL_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "${SHADER_SRC}")

    set(SPV_OUT   "${SHADER_BINARY_DIR}/${REL_PATH}.spv")
    set(DEP_FILE  "${SHADER_BINARY_DIR}/${REL_PATH}.d")
    # Stamp file: only touched when glslc actually writes a new SPV.
    # CMake uses the stamp's mtime to decide whether downstream targets are stale,
    # so this is what prevents re-linking the executable on every build.
    set(STAMP     "${SHADER_STAMP_DIR}/${REL_PATH}.stamp")

    get_filename_component(SPV_DIR   "${SPV_OUT}" DIRECTORY)
    get_filename_component(STAMP_DIR "${STAMP}"   DIRECTORY)
    file(MAKE_DIRECTORY "${SPV_DIR}" "${STAMP_DIR}")

    add_custom_command(
            OUTPUT  "${SPV_OUT}" "${STAMP}"
            COMMAND "${GLSLC}"
            -g
            -O
            -MD -MF "${DEP_FILE}"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            --target-env=vulkan1.3
            "${SHADER_SRC}"
            -o "${SPV_OUT}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SPV_OUT}" "${SPV_OUT}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${STAMP}"
            DEPENDS "${SHADER_SRC}"
            DEPFILE "${DEP_FILE}"
            COMMENT "glslc  ${REL_PATH}  ->  ${REL_PATH}.spv"
            VERBATIM
    )

    list(APPEND SHADER_OUTPUTS "${SPV_OUT}")
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

# ---------------------------------------------------------------------------
# Engine executable
# ---------------------------------------------------------------------------
set(CORE_SOURCES
        src/Core/main.cpp
        src/Core/Window.cpp
        src/Core/Application.cpp
        src/Core/Device.cpp
        src/Core/Buffer.cpp
        src/Core/JobSystem.h
        src/Core/Descriptor.cpp
        src/Core/Descriptor.h
)

set(RENDERER_SOURCES
        src/Renderer/Renderer.cpp
        src/Renderer/Swapchain.cpp
        src/Renderer/RenderGraph.cpp
        src/Renderer/ResourceHeap.cpp
        src/Renderer/ShaderUtils.h
)

set(SCENE_SOURCES
        src/Scene/Texture.cpp
        src/Scene/Model.h
        src/Scene/Model.cpp
        src/Scene/GameObject.cpp
        src/Scene/GameObject.h
        src/Scene/Camera.cpp
        src/Scene/LoaderGLTF.cpp
        src/Scene/LoaderGLTF.h
        src/Scene/KeyboardMovement.cpp
        src/Scene/IBL.cpp
        src/Scene/IBL.h
)

set(PASSES_SOURCES
        src/Passes/GBufferPassNode.cpp
        src/Passes/GBufferPassNode.h
        src/Passes/FxaaPassNode.h
        src/Passes/FxaaPassNode.cpp
        src/Passes/LightPassNode.cpp
        src/Passes/LightPassNode.h
        src/Passes/SsaoPassNode.cpp
        src/Passes/SsaoPassNode.h
)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/models" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

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
        fastgltf
        stb
)

add_dependencies(RenderEngine Shaders)