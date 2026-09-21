#include "Vulkan/VulkanPipelineCache.h"
#include "Vulkan/VulkanDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/Hash.h"

#include <fstream>
#include <vector>
#include <array>

namespace Engine {

    VulkanPipelineCache::VulkanPipelineCache(VulkanDevice& device, VkDescriptorSetLayout bindlessLayout)
        : device(device),
          bindlessLayout(bindlessLayout)
    {
        VkPipelineCacheCreateInfo cacheCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .initialDataSize = 0,
            .pInitialData = nullptr
        };
        VkResult res = vkCreatePipelineCache(device.GetHandle(), &cacheCreateInfo, nullptr, &pipelineCache);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create VkPipelineCache!");
    }

    VulkanPipelineCache::~VulkanPipelineCache()
    {
        pipelineMap.clear();

        if (pipelineCache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(device.GetHandle(), pipelineCache, nullptr);
            pipelineCache = VK_NULL_HANDLE;
        }
    }

    std::vector<char> VulkanPipelineCache::ReadShaderFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            const std::vector<std::string> fallbacks = {
                "../" + path,
                "../../" + path,
                "shaders/" + path
            };
            for (const auto& alt : fallbacks) {
                file.clear();
                file.open(alt, std::ios::ate | std::ios::binary);
                if (file.is_open()) {
                    break;
                }
            }
        }

        ENGINE_VERIFY(file.is_open(), "Failed to open shader file: {}", path);

        const size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
        file.close();
        return buffer;
    }

    VkShaderModule VulkanPipelineCache::CreateShaderModule(const std::string& path) const
    {
        auto code = ReadShaderFile(path);
        ENGINE_VERIFY(code.size() % 4 == 0, "Shader bytecode size must be multiple of 4: {}", path);

        VkShaderModuleCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VkResult res = vkCreateShaderModule(device.GetHandle(), &createInfo, nullptr, &shaderModule);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create shader module for: {}", path);

        return shaderModule;
    }

    uint64_t VulkanPipelineCache::HashComputeDesc(const ComputePipelineDesc& desc) const
    {
        uint64_t hash = Hash64(desc.computeShaderPath);
        HashCombine(hash, desc.pushConstantSize);
        return hash;
    }

    uint64_t VulkanPipelineCache::HashGraphicsDesc(const GraphicsPipelineDesc& desc) const
    {
        uint64_t hash = Hash64(desc.vertexShaderPath);
        HashCombine(hash, Hash64(desc.fragmentShaderPath));
        HashCombine(hash, Hash64(desc.taskShaderPath));
        HashCombine(hash, Hash64(desc.meshShaderPath));
        HashCombine(hash, static_cast<uint32_t>(desc.cullMode));
        HashCombine(hash, desc.depthTest);
        HashCombine(hash, desc.depthWrite);
        HashCombine(hash, static_cast<uint32_t>(desc.depthCompareOp));
        HashCombine(hash, static_cast<uint32_t>(desc.blendMode));
        for (VkFormat format : desc.colorAttachmentFormats) {
            HashCombine(hash, static_cast<uint32_t>(format));
        }
        HashCombine(hash, static_cast<uint32_t>(desc.depthAttachmentFormat));
        HashCombine(hash, desc.pushConstantSize);
        return hash;
    }

    std::shared_ptr<VulkanPipeline> VulkanPipelineCache::GetOrCreateComputePipeline(const ComputePipelineDesc& desc)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        uint64_t hash = HashComputeDesc(desc);
        auto it = pipelineMap.find(hash);
        if (it != pipelineMap.end()) {
            return it->second;
        }

        VkShaderModule compModule = CreateShaderModule(desc.computeShaderPath);

        VkPipelineShaderStageCreateInfo stageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        VkPushConstantRange pushRange{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = desc.pushConstantSize
        };

        std::vector<VkDescriptorSetLayout> setLayouts;
        if (bindlessLayout != VK_NULL_HANDLE) {
            setLayouts.push_back(bindlessLayout);
        }

        VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
            .pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data(),
            .pushConstantRangeCount = desc.pushConstantSize > 0 ? 1u : 0u,
            .pPushConstantRanges = desc.pushConstantSize > 0 ? &pushRange : nullptr
        };

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkResult res = vkCreatePipelineLayout(device.GetHandle(), &layoutInfo, nullptr, &pipelineLayout);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create compute pipeline layout for: {}", desc.debugName);

        VkComputePipelineCreateInfo computePipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stageInfo,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        res = vkCreateComputePipelines(device.GetHandle(), pipelineCache, 1, &computePipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(device.GetHandle(), compModule, nullptr);

        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create compute pipeline for: {}", desc.debugName);

        if (!desc.debugName.empty()) {
            device.SetObjectName(pipeline, desc.debugName);
        }

        auto pso = std::make_shared<VulkanPipeline>(device, pipeline, pipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE);
        pipelineMap[hash] = pso;
        computeDescs[hash] = desc;
        return pso;
    }

    std::shared_ptr<VulkanPipeline> VulkanPipelineCache::GetOrCreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        uint64_t hash = HashGraphicsDesc(desc);
        auto it = pipelineMap.find(hash);
        if (it != pipelineMap.end()) {
            return it->second;
        }

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<VkShaderModule> modulesToDestroy;

        auto AddStage = [&](const std::string& path, VkShaderStageFlagBits stageFlag) {
            if (path.empty()) return;
            VkShaderModule module = CreateShaderModule(path);
            modulesToDestroy.push_back(module);
            stages.push_back(VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = stageFlag,
                .module = module,
                .pName = "main",
                .pSpecializationInfo = nullptr
            });
        };

        const bool hasMeshShader = !desc.meshShaderPath.empty();
        if (hasMeshShader) {
            AddStage(desc.taskShaderPath, VK_SHADER_STAGE_TASK_BIT_EXT);
            AddStage(desc.meshShaderPath, VK_SHADER_STAGE_MESH_BIT_EXT);
        } else {
            AddStage(desc.vertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
        }
        AddStage(desc.fragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);

        ENGINE_VERIFY(!stages.empty(), "No valid shader stages provided for pipeline: {}", desc.debugName);

        VkShaderStageFlags pushStageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        if (!desc.taskShaderPath.empty()) {
            pushStageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT;
        }
        if (!desc.meshShaderPath.empty()) {
            pushStageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
        }

        VkPushConstantRange pushRange{
            .stageFlags = pushStageFlags,
            .offset = 0,
            .size = desc.pushConstantSize
        };

        std::vector<VkDescriptorSetLayout> setLayouts;
        if (bindlessLayout != VK_NULL_HANDLE) {
            setLayouts.push_back(bindlessLayout);
        }

        VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
            .pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data(),
            .pushConstantRangeCount = desc.pushConstantSize > 0 ? 1u : 0u,
            .pPushConstantRanges = desc.pushConstantSize > 0 ? &pushRange : nullptr
        };

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkResult res = vkCreatePipelineLayout(device.GetHandle(), &layoutInfo, nullptr, &pipelineLayout);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create graphics pipeline layout for: {}", desc.debugName);

        VkPipelineRenderingCreateInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(desc.colorAttachmentFormats.size()),
            .pColorAttachmentFormats = desc.colorAttachmentFormats.empty() ? nullptr : desc.colorAttachmentFormats.data(),
            .depthAttachmentFormat = desc.depthAttachmentFormat,
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
        };

        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = nullptr,
            .scissorCount = 1,
            .pScissors = nullptr
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = static_cast<VkCullModeFlags>(desc.cullMode),
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };

        VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        VkPipelineDepthStencilStateCreateInfo depthStencil{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE,
            .depthCompareOp = static_cast<VkCompareOp>(desc.depthCompareOp),
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(desc.colorAttachmentFormats.size());
        for (size_t i = 0; i < desc.colorAttachmentFormats.size(); ++i) {
            auto& blend = blendAttachments[i];
            blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            switch (desc.blendMode) {
                case BlendMode::Opaque:
                    blend.blendEnable = VK_FALSE;
                    break;
                case BlendMode::AlphaBlend:
                    blend.blendEnable = VK_TRUE;
                    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    blend.colorBlendOp = VK_BLEND_OP_ADD;
                    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                    blend.alphaBlendOp = VK_BLEND_OP_ADD;
                    break;
                case BlendMode::Additive:
                    blend.blendEnable = VK_TRUE;
                    blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.colorBlendOp = VK_BLEND_OP_ADD;
                    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    blend.alphaBlendOp = VK_BLEND_OP_ADD;
                    break;
            }
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
            .pAttachments = blendAttachments.empty() ? nullptr : blendAttachments.data(),
            .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
        };

        VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &renderingInfo,
            .flags = 0,
            .stageCount = static_cast<uint32_t>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = hasMeshShader ? nullptr : &vertexInputInfo,
            .pInputAssemblyState = hasMeshShader ? nullptr : &inputAssembly,
            .pTessellationState = nullptr,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = pipelineLayout,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        res = vkCreateGraphicsPipelines(device.GetHandle(), pipelineCache, 1, &pipelineInfo, nullptr, &pipeline);

        for (VkShaderModule mod : modulesToDestroy) {
            vkDestroyShaderModule(device.GetHandle(), mod, nullptr);
        }

        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create graphics pipeline for: {}", desc.debugName);

        if (!desc.debugName.empty()) {
            device.SetObjectName(pipeline, desc.debugName);
        }

        auto pso = std::make_shared<VulkanPipeline>(device, pipeline, pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS);
        pipelineMap[hash] = pso;
        graphicsDescs[hash] = desc;
        return pso;
    }

    void VulkanPipelineCache::ReloadAllShaders()
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        pipelineMap.clear();
        LOG_INFO("VulkanPipelineCache", "Shader cache invalidated. Pipelines will be recompiled on next demand.");
    }

} // namespace Engine
