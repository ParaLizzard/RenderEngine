#include <gtest/gtest.h>

#include "Integration/RenderPassIntegrationTest.h"
#include "Renderer/Passes/FxaaPassNode.h"
#include "Renderer/Passes/SsaoPassNode.h"
#include "Renderer/Passes/VisibilityPassNode.h"
#include "Renderer/Passes/MaterialPassNode.h"
#include "Renderer/Passes/CullPassNode.h"
#include "Core/EngineConstants.h"
#include "Vulkan/Buffer.h"

// =============================================================================
// FxaaPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, FxaaPassInitializesAndCompiles) {
    ASSERT_NO_THROW({
        Engine::FxaaPassNode fxaaPass(*device, *renderer, *megaBuffer, *resourceHeap);
        renderGraph->addPass(&fxaaPass);
        
        // Register required fake resources to satisfy the RenderGraph compilation
        renderGraph->registerPhysicalImage("TonemapOutput",
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
        Engine::CullPassNode cullPass(*device, *renderer, *megaBuffer, *resourceHeap);
        renderGraph->addPass(&cullPass);
        
        renderGraph->registerPhysicalImage("HiZImage",
                                          renderer->getSwapChain().getDepthImage(), renderer->getSwapChain().getDepthImageView(),
                                          renderer->getSwapChain().getDepthFormat(), {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);

        std::unique_ptr<Engine::Buffer> dummyBuf1;
        std::unique_ptr<Engine::Buffer> dummyBuf2;
        if (!device->isMeshShaderSupported()) {
            dummyBuf1 = std::make_unique<Engine::Buffer>(*device, sizeof(uint32_t) * 100, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
            dummyBuf2 = std::make_unique<Engine::Buffer>(*device, sizeof(VkDrawIndexedIndirectCommand) * 100, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
            
            renderGraph->registerPhysicalBuffer("CompactedIndexBuffer", dummyBuf1->getBuffer(), 400, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
            renderGraph->registerPhysicalBuffer("SingleIndirectCommand", dummyBuf2->getBuffer(), sizeof(VkDrawIndexedIndirectCommand) * 100, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
            renderGraph->registerPhysicalBuffer("MaskedCompactedIndexBuffer", dummyBuf1->getBuffer(), 400, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
            renderGraph->registerPhysicalBuffer("MaskedSingleIndirectCommand", dummyBuf2->getBuffer(), sizeof(VkDrawIndexedIndirectCommand) * 100, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        }
                                            
        renderGraph->compile();
    }) << "CullPass failed to initialize.";
}

// =============================================================================
// VisibilityPassNode Integration
// =============================================================================

TEST_F(RenderPassIntegrationTest, VisibilityPassInitializesAndCompiles) {
    ASSERT_NO_THROW({
        Engine::CullPassNode cullPass(*device, *renderer, *megaBuffer, *resourceHeap);
        Engine::VisibilityPassNode visPass(*device, *renderer, *megaBuffer, cullPass, *resourceHeap);
        
        renderGraph->addPass(&visPass);
        
        Engine::Buffer dummyBuffer1(*device, Engine::Constants::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        Engine::Buffer dummyBuffer2(*device, sizeof(uint32_t), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 1);
        
        renderGraph->registerPhysicalBuffer("CullCompactedIndirectCommands",
                                            dummyBuffer1.getBuffer(), Engine::Constants::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand),
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
        Engine::CullPassNode cullPass(*device, *renderer, *megaBuffer, *resourceHeap);
        Engine::MaterialPassNode matPass(*device, *renderer, *megaBuffer, *resourceHeap, cullPass, *renderGraph);
    }) << "MaterialPass failed to initialize compute pipelines.";
}

