#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/fwd.hpp>
#include <glm/gtx/transform.hpp>

#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"

namespace Engine {
    class GameObject;
    class TextureCubeMap;
    class ResourceHeap;
    class Model;
    class Device;
    struct TextureData
    {
        VkImage image;
        VkImageView imageView;
        VkSampler sampler;
        uint32_t mipLevels;
    };

    class IBL
    {
    public:
        IBL(Device &device,
            TextureCubeMap &skyboxTexture,
            ResourceHeap &resourceHeap,
            Model &megaBuffer,
            GameObject &cube);
        ~IBL();

        IBL(const IBL &) = default;
        IBL &operator=(const IBL &) = delete;

        TextureData BRDFLUT;
        TextureData irradianceCube;
        TextureData prefilteredCube;

    private:
        Device &device;
        TextureCubeMap &skyboxTexture;
        ResourceHeap &resourceHeap;

        void generateBRDFLUT();

        VmaAllocation BRDFallocation;
        const VkFormat formatBRDF = VK_FORMAT_R16G16_SFLOAT;
        const int32_t dimBRDF = 512;

        void generateIrradiance();

        VmaAllocation irradianceAllocation;
        const int32_t dimIrradiance = 64;

        void generatePrefilter();

        VmaAllocation prefilterAllocation;
        const int32_t dimPrefilter = 512;

        GameObject &cube;
        Model &megaBuffer;
    };
} // namespace Engine
