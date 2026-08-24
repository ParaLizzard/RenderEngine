set(TEST_SOURCES
        TestMain.cpp

        # Common Test Harness & Synthetic Generators
        Common/VulkanTestContext.cpp
        Common/SyntheticMeshGenerator.cpp
        Common/SyntheticTextureGenerator.cpp
        Common/TestContextTest.cpp
        Common/SyntheticGeneratorsTest.cpp

        # Vulkan
        Vulkan/BufferAlignmentTest.cpp
        Vulkan/BufferDescriptorInfoTest.cpp
        Vulkan/VkUtilsTest.cpp
        Vulkan/QueueFamilyIndicesTest.cpp

        # Renderer
        Renderer/RenderGraphBuilderTest.cpp
        Renderer/RenderGraphRegistryTest.cpp
        Renderer/RenderGraphCompileTest.cpp
        Renderer/RenderGraphExecutionTest.cpp
        Renderer/RenderGraphLifecycleTest.cpp
        Renderer/ShaderUtilsTest.cpp
        Renderer/PerformanceMonitorTest.cpp
        Renderer/FrameInfoStructTest.cpp

        # Scene
        Scene/CameraProjectionTest.cpp
        Scene/CameraViewTest.cpp

        # Core
        Core/ExceptionTest.cpp
        Core/EngineConfigTest.cpp
        Core/EngineConstantsTest.cpp
        Core/TypesHashTest.cpp
        Core/LogTest.cpp
        Core/ClockTest.cpp
        Core/CVarTest.cpp
        
        # Integration (GPU required)
        Integration/BaselineCaptureTest.cpp
        Integration/PassesTest.cpp
)

add_executable(RenderEngineTests ${TEST_SOURCES})

target_link_libraries(RenderEngineTests PRIVATE
        RenderEngineLib
        GTest::gtest
        GTest::gmock
)

target_include_directories(RenderEngineTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(RenderEngineTests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

