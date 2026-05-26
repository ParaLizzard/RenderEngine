#pragma once
#include <cstdint>
#include <mutex>

#include "Device.h"
#include <vector>

namespace Engine
{
    class ResourceHeap
    {
        struct PendingWrite {
            uint32_t dstArrayElement;
            VkDescriptorImageInfo imageInfo;
        };

    public:
        struct TextureHandle {
            uint32_t index;
            uint32_t generation;

            bool operator==(const TextureHandle& other) const {
                return index == other.index && generation == other.generation;
            }
        };

        ResourceHeap(Device& device, uint32_t maxTextures = 4096);
        ~ResourceHeap();

        ResourceHeap(ResourceHeap const&) = delete;
        ResourceHeap& operator=(ResourceHeap const&) = delete;

        TextureHandle registerTexture(VkDescriptorImageInfo imageInfo);
        void freeTexture(TextureHandle& handle);

        void flushPendingUpdates();
        bool isSlotAllocated(TextureHandle& handle) const;

        VkDescriptorSet getDescriptorSet() { return globalDescriptorSet; }
        VkDescriptorSetLayout getDescriptorSetLayout() { return globalDescriptorSetLayout; }
    private:
        struct SlotMetadata {
            bool allocated = false;
            uint32_t generation = 0;
        };

        Device& device;

        uint32_t maxDescriptors;
        VkDescriptorPool globalDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout globalDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet globalDescriptorSet = VK_NULL_HANDLE;


        std::vector<SlotMetadata> slots;
        std::vector<PendingWrite> pendingWrites;
        std::vector<uint32_t> freeIndices;

        bool hasPendingWrites = false;

        mutable std::mutex heapMutex;
    };
}