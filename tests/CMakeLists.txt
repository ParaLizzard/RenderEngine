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
        Vulkan/VulkanDeviceInitTest.cpp
        Vulkan/VulkanMemoryBudgetTest.cpp

        # Renderer
        Renderer/RenderSettingsTest.cpp
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
        Core/AssertTest.cpp
        Core/EngineConstantsTest.cpp
        Core/TypesHashTest.cpp
        Core/LogTest.cpp
        Core/ClockTest.cpp
        Core/CVarTest.cpp
        Core/ConfigTest.cpp
        Core/SubsystemRegistryTest.cpp
        Core/EngineLifecycleTest.cpp

        # Threading
        Threading/WorkStealingQueueTest.cpp
        Threading/JobSystemTest.cpp

        # System
        System/EventDispatcherTest.cpp
        System/WindowSubsystemTest.cpp
        System/InputSubsystemTest.cpp
        
        # AssetSystem
        AssetSystem/AssetHandleTest.cpp

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

# ---------------------------------------------------------------------------
# Step 2.1 — VulkanDeviceInitTest Standalone Test Executable
# ---------------------------------------------------------------------------
add_executable(VulkanDeviceInitTest
        TestMain.cpp
        Common/VulkanTestContext.cpp
        Vulkan/VulkanDeviceInitTest.cpp
        Vulkan/QueueFamilyIndicesTest.cpp
        ${CMAKE_SOURCE_DIR}/src/Vulkan/VulkanDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/Vulkan/VulkanMemory.cpp
        ${CMAKE_SOURCE_DIR}/src/Core/Log.cpp
)

target_include_directories(VulkanDeviceInitTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/_deps/ktx-src/other_include
)

target_link_libraries(VulkanDeviceInitTest PRIVATE
        Vulkan::Vulkan
        GTest::gtest
        GTest::gmock
)

gtest_discover_tests(VulkanDeviceInitTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Step 2.2 — VulkanMemoryBudgetTest Standalone Test Executable
# ---------------------------------------------------------------------------
add_executable(VulkanMemoryBudgetTest
        TestMain.cpp
        Common/VulkanTestContext.cpp
        Vulkan/VulkanMemoryBudgetTest.cpp
        ${CMAKE_SOURCE_DIR}/src/Vulkan/VulkanDevice.cpp
        ${CMAKE_SOURCE_DIR}/src/Vulkan/VulkanMemory.cpp
        ${CMAKE_SOURCE_DIR}/src/Core/Log.cpp
)

target_include_directories(VulkanMemoryBudgetTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/_deps/ktx-src/other_include
)

target_link_libraries(VulkanMemoryBudgetTest PRIVATE
        Vulkan::Vulkan
        GTest::gtest
        GTest::gmock
)

gtest_discover_tests(VulkanMemoryBudgetTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Step 2.9 & 2.10 — ThreadingTests Standalone Test Executable
# ---------------------------------------------------------------------------
add_executable(ThreadingTests
        TestMain.cpp
        Threading/WorkStealingQueueTest.cpp
        Threading/JobSystemTest.cpp
        ${CMAKE_SOURCE_DIR}/src/Threading/JobSystem.cpp
        ${CMAKE_SOURCE_DIR}/src/Threading/JobSubsystem.cpp
        ${CMAKE_SOURCE_DIR}/src/Threading/TaskGraph.cpp
        ${CMAKE_SOURCE_DIR}/src/Core/Log.cpp
        ${CMAKE_SOURCE_DIR}/src/Core/SubsystemRegistry.cpp
)

target_include_directories(ThreadingTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ThreadingTests PRIVATE
        GTest::gtest
        GTest::gmock
)

gtest_discover_tests(ThreadingTests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Step 3.1 — AssetSystemTests Standalone Test Executable
# ---------------------------------------------------------------------------
add_executable(AssetSystemTests
        TestMain.cpp
        AssetSystem/AssetHandleTest.cpp
        ${CMAKE_SOURCE_DIR}/src/Core/UUID.cpp
        ${CMAKE_SOURCE_DIR}/src/Core/Log.cpp
)

target_include_directories(AssetSystemTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(AssetSystemTests PRIVATE
        GTest::gtest
        GTest::gmock
)

gtest_discover_tests(AssetSystemTests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
