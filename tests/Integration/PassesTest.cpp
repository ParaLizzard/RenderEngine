#include <gtest/gtest.h>

#include "Integration/RenderPassIntegrationTest.h"
#include "Renderer/Passes/FxaaPassNode.h"
#include "Renderer/Passes/SsaoPassNode.h"
#include "Renderer/Passes/VisibilityPassNode.h"
#include "Renderer/Passes/MaterialPassNode.h"
#include "Renderer/Passes/CullPassNode.h"
#include "Core/EngineConfig.h"
#include "Vulkan/Buffer.h"

// =============================================================================
// FxaaPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, FxaaPassInitializesAndCompiles) {
    ASSERT_NO_THROW({
        Engine::FxaaPassNode fxaaPass(*device, *renderer, *megaBuffer, *resourceHeap);
        renderGraph->addPass(&fxaaPass);
        
        // Register required fake resources to satisfy the RenderGraph compilation
        renderGraph->registerPhysicalImage("FinalRender",
                                          renderer->getSwapChain().getImage(0), renderer->getSwapChain().getImageView(0),
                                          VK_FORMAT_R8G8B8A8_UNORM, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
        renderGraph->registerPhysicalImage("SwapChainImage",
                                          renderer->getSwapChain().getImage(0), renderer->getSwapChain().getImageView(0),
                                          renderer->getSwapChain().getSwapChainImageFormat(), {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
                                          
        renderGraph->compile();
    }) << "FxaaPass failed to initialize or compile into the graph. Check shader loading or pipeline creation.";
}

// =============================================================================
// SsaoPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, SsaoPassInitializesAndCompiles) {
    ASSERT_NO_THROW({
        Engine::SsaoPassNode ssaoPass(*device, *renderer, *megaBuffer, *resourceHeap);
        renderGraph->addPass(&ssaoPass);
        
        Engine::Buffer dummyBuffer(*device, 800 * 600 * sizeof(uint32_t), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        
        // Register required fake resources
        renderGraph->registerPhysicalImage("DepthImage",
                                          renderer->getSwapChain().getDepthImage(), renderer->getSwapChain().getDepthImageView(),
                                          renderer->getSwapChain().getDepthFormat(), {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
        
        // The SSAO pass also uses PackedNormals which is a buffer
        renderGraph->registerPhysicalBuffer("PackedNormals",
                                            dummyBuffer.getBuffer(), 800 * 600 * sizeof(uint32_t),
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
                                            
        renderGraph->registerPhysicalImage("SsaoBlurImage",
                                          renderer->getSwapChain().getImage(0), renderer->getSwapChain().getImageView(0),
                                          VK_FORMAT_R8G8B8A8_UNORM, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
                                          
        renderGraph->compile();
    }) << "SsaoPass failed to initialize. Check if shaders exist and pipeline creates.";
}

// =============================================================================
// CullPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, CullPassInitializesAndCompiles) {
    ASSERT_NO_THROW({
        Engine::CullPassNode cullPass(*device, *renderer, *megaBuffer);
        renderGraph->addPass(&cullPass);
        
        Engine::Buffer dummyBuffer(*device, Engine::Config::MAX_SCENE_OBJECTS * sizeof(Engine::ObjectData), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        
        renderGraph->registerPhysicalBuffer("CullObjectData",
                                            dummyBuffer.getBuffer(), Engine::Config::MAX_SCENE_OBJECTS * sizeof(Engine::ObjectData),
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
                                            
        Engine::Buffer dummyBuffer2(*device, Engine::Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        Engine::Buffer dummyBuffer3(*device, sizeof(uint32_t), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        
        renderGraph->registerPhysicalBuffer("CullCompactedIndirectCommands",
                                            dummyBuffer2.getBuffer(), Engine::Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand),
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph->registerPhysicalBuffer("CullDrawCount",
                                            dummyBuffer3.getBuffer(), sizeof(uint32_t),
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
                                            
        renderGraph->compile();
    }) << "CullPass failed to initialize.";
}

// =============================================================================
// VisibilityPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, VisibilityPassInitializesAndCompiles) {
    ASSERT_NO_THROW({
        Engine::CullPassNode cullPass(*device, *renderer, *megaBuffer);
        Engine::VisibilityPassNode visPass(*device, *renderer, *megaBuffer, cullPass);
        
        renderGraph->addPass(&visPass);
        
        Engine::Buffer dummyBuffer1(*device, Engine::Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        Engine::Buffer dummyBuffer2(*device, sizeof(uint32_t), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        
        renderGraph->registerPhysicalBuffer("CullCompactedIndirectCommands",
                                            dummyBuffer1.getBuffer(), Engine::Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand),
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph->registerPhysicalBuffer("CullDrawCount",
                                            dummyBuffer2.getBuffer(), sizeof(uint32_t),
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
                                            
        renderGraph->registerPhysicalImage("DepthImage",
                                          renderer->getSwapChain().getDepthImage(), renderer->getSwapChain().getDepthImageView(),
                                          renderer->getSwapChain().getDepthFormat(), {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
                                          
        renderGraph->compile();
    }) << "VisibilityPass failed to initialize.";
}

// =============================================================================
// MaterialPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, MaterialPassInitializes) {
    // We only test initialization because compiling MaterialPass requires many buffers
    ASSERT_NO_THROW({
        Engine::MaterialPassNode matPass(*device, *renderer, *megaBuffer, *resourceHeap, *renderGraph);
    }) << "MaterialPass failed to initialize compute pipelines.";
}
