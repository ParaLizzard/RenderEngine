#pragma once
#include <string>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <cstdint>
#include "Vulkan/VulkanBindlessHeap.h"

namespace Engine {
    enum class AlphaMode : uint32_t {
        Opaque = 0,
        Mask   = 1,
        Blend  = 2
    };

    struct PBRMaterialDesc {
        std::string name;
        glm::vec4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 emissiveFactor{ 0.0f, 0.0f, 0.0f };
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        float alphaCutoff = 0.5f;
        AlphaMode alphaMode = AlphaMode::Opaque;
        bool doubleSided = false;

        std::string albedoTexturePath;
        std::string normalTexturePath;
        std::string metallicRoughnessTexturePath;
        std::string emissiveTexturePath;
        std::string occlusionTexturePath;

        int32_t albedoTextureIndex = -1;
        int32_t normalTextureIndex = -1;
        int32_t metallicRoughnessTextureIndex = -1;
        int32_t emissiveTextureIndex = -1;
        int32_t occlusionTextureIndex = -1;
    };

    struct alignas(16) MaterialGPU {
        glm::vec4 baseColorFactor;
        glm::vec3 emissiveFactor;
        float roughnessFactor;
        float metallicFactor;
        float alphaCutoff;
        uint32_t alphaMode;
        uint32_t flags;

        uint32_t albedoSlot;
        uint32_t normalSlot;
        uint32_t metallicRoughnessSlot;
        uint32_t emissiveSlot;
        uint32_t occlusionSlot;
        uint32_t padding[3];
    };
}