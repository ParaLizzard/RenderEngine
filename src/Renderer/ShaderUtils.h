#pragma once
#include <fstream>
#include <string>
#include "Core/Assert.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace Engine::ShaderUtils {
    static std::vector<char> readFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::vector<std::string> fallbackPaths = {
                "../" + filename,
                "../../" + filename
            };
            for (const auto &altPath : fallbackPaths) {
                file.clear();
                file.open(altPath, std::ios::ate | std::ios::binary);
                if (file.is_open()) {
                    break;
                }
            }
        }

        ENGINE_VERIFY(file.is_open(), "failed to open file: {}", filename);
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    static VkShaderModule createShaderModule(VkDevice device, const std::vector<char> &code)
    {
        VkShaderModuleCreateInfo createInfo {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
        VkShaderModule shaderModule;
        ENGINE_VERIFY(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) == VK_SUCCESS,
            "failed to create shader module");
        return shaderModule;
    }
} // namespace Engine::ShaderUtils
