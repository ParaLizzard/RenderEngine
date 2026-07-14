#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>

#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include "Core/Device.h"

#include "Core/Buffer.h"
#include "Core/EngineConfig.h"
#include "Renderer/Renderer.h"


namespace Engine {
    class Texture2D;

    class ResourceHeap
    {
        struct PendingWrite
        {
            uint32_t dstArrayElement;
            VkDescriptorImageInfo imageInfo;
        };

    public:
        static constexpr uint32_t INVALID_HANDLE = 0xFFFFFFFFu;

        struct TextureHandle
        {
            uint32_t index;
            uint32_t generation;

            bool operator==(const TextureHandle &other) const
            {
                return index == other.index && generation == other.generation;
            }
        };

        struct MaterialData
        {
            glm::vec4 albedoFactor;
            glm::vec4 emissiveFactor;

            uint32_t albedoIndex;
            uint32_t normalIndex;
            uint32_t roughnessMetallicIndex;
            uint32_t emissiveIndex;
            uint32_t occlusionIndex;
            uint32_t flags;
            float alphaCutoff;
            float normalScale;
            float roughnessFactor;
            float metallicFactor;

            uint32_t padding[2];
        };

        ResourceHeap(Device &device, uint32_t maxTextures = 4096);
        ~ResourceHeap();

        ResourceHeap(ResourceHeap const &) = delete;
        ResourceHeap &operator=(ResourceHeap const &) = delete;

        TextureHandle registerTexture(VkDescriptorImageInfo imageInfo);
        void freeTexture(TextureHandle &handle);

        const std::vector<MaterialData> &getMaterials() const
        {
            return materials;
        }

        void flushPendingUpdates();
        bool isSlotAllocated(TextureHandle &handle) const;
        uint32_t pushMaterial(const MaterialData &mat);
        uint32_t getFallbackWhiteSlot() const
        {
            return fallbackWhiteSlot;
        }
        uint32_t getFallbackFlatNormalSlot() const
        {
            return fallbackFlatNormalSlot;
        }

        void uploadMaterialBuffer(uint32_t currentFrame);

        VkDescriptorBufferInfo getMaterialBufferInfo(uint32_t currentFrame) const;
        void writeMaterialDescriptor(uint32_t currentFrame);
        void writeMaterialDescriptorAllFrames(); // For init

        void markMaterialsDirty()
        {
            materialFramesToUpdate = Config::MAX_FRAMES_IN_FLIGHT;
        }
        void update(uint32_t currentFrame);
        void writeSceneUboDescriptor(VkDescriptorBufferInfo bufInfo, uint32_t frameIdx);
        void writeIBLDescriptors(VkDescriptorImageInfo irradianceInfo,
                                 VkDescriptorImageInfo prefilterInfo,
                                 VkDescriptorImageInfo brdfLutInfo);
        VkDeviceSize getMaterialBufferSize() const
        {
            return sizeof(MaterialData) * materials.size();
        }

        VkDescriptorSet getDescriptorSet(uint32_t frameIdx)
        {
            return globalDescriptorSets[frameIdx];
        }
        VkDescriptorSetLayout getDescriptorSetLayout()
        {
            return globalDescriptorSetLayout;
        }

    private:
        uint32_t fallbackWhiteSlot = 0;
        uint32_t fallbackFlatNormalSlot = 1;

        struct SlotMetadata
        {
            bool allocated = false;
            uint32_t generation = 0;
        };

        Device &device;

        uint32_t maxDescriptors;
        VkDescriptorPool globalDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout globalDescriptorSetLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> globalDescriptorSets;

        std::unique_ptr<Texture2D> fallbackWhiteTex;
        std::unique_ptr<Texture2D> fallbackFlatNormalTex;

        std::vector<std::unique_ptr<Buffer>> materialBuffers;

        std::vector<SlotMetadata> slots;
        std::vector<PendingWrite> pendingWrites;
        std::vector<uint32_t> freeIndices;
        std::vector<MaterialData> materials;

        bool hasPendingWrites = false;
        uint32_t materialFramesToUpdate = 0;

        mutable std::mutex heapMutex;
    };
} // namespace Engine
