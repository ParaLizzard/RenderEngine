#include "Vulkan/VulkanSamplers.h"
#include "Vulkan/VulkanDevice.h"

#include <algorithm>
#include "Core/Assert.h"

namespace Engine {

    VulkanSamplers::VulkanSamplers(const VulkanDevice& device)
        : device(&device)
    {
        CreateSamplers();
    }

    VulkanSamplers::~VulkanSamplers()
    {
        DestroySamplers();
    }

    VulkanSamplers::VulkanSamplers(VulkanSamplers&& other) noexcept
        : device(other.device),
          samplers(other.samplers)
    {
        other.samplers.fill(VK_NULL_HANDLE);
        other.device = nullptr;
    }

    VulkanSamplers& VulkanSamplers::operator=(VulkanSamplers&& other) noexcept
    {
        if (this != &other) {
            DestroySamplers();

            device = other.device;
            samplers = other.samplers;

            other.samplers.fill(VK_NULL_HANDLE);
            other.device = nullptr;
        }
        return *this;
    }

    VkSampler VulkanSamplers::GetSampler(SamplerType type) const noexcept
    {
        auto index = static_cast<size_t>(type);
        ENGINE_ASSERT(index < static_cast<size_t>(SamplerType::Count), "VulkanSamplers: Invalid sampler type requested!");
        return samplers[index];
    }

    VkSamplerCreateInfo VulkanSamplers::GetSamplerCreateInfo(SamplerType type, float maxAnisotropy)
    {
        VkSamplerCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };

        switch (type) {
            case SamplerType::PointClamp:
                info.magFilter = VK_FILTER_NEAREST;
                info.minFilter = VK_FILTER_NEAREST;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                break;

            case SamplerType::PointWrap:
                info.magFilter = VK_FILTER_NEAREST;
                info.minFilter = VK_FILTER_NEAREST;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;

            case SamplerType::LinearClamp:
                info.magFilter = VK_FILTER_LINEAR;
                info.minFilter = VK_FILTER_LINEAR;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                break;

            case SamplerType::LinearWrap:
                info.magFilter = VK_FILTER_LINEAR;
                info.minFilter = VK_FILTER_LINEAR;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;

            case SamplerType::Aniso16xClamp:
                info.magFilter = VK_FILTER_LINEAR;
                info.minFilter = VK_FILTER_LINEAR;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                info.anisotropyEnable = VK_TRUE;
                info.maxAnisotropy = std::min(16.0f, maxAnisotropy);
                break;

            case SamplerType::Aniso16xWrap:
                info.magFilter = VK_FILTER_LINEAR;
                info.minFilter = VK_FILTER_LINEAR;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                info.anisotropyEnable = VK_TRUE;
                info.maxAnisotropy = std::min(16.0f, maxAnisotropy);
                break;

            case SamplerType::ShadowCompare:
                info.magFilter = VK_FILTER_LINEAR;
                info.minFilter = VK_FILTER_LINEAR;
                info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
                info.compareEnable = VK_TRUE;
                info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                break;

            default:
                break;
        }

        return info;
    }

    void VulkanSamplers::CreateSamplers()
    {
        ENGINE_ASSERT(device != nullptr, "VulkanSamplers: Device is null during creation!");

        float maxAniso = device->GetCapabilities().maxSamplerAnisotropy;

        static constexpr const char* samplerNames[] = {
            "Sampler_PointClamp",
            "Sampler_PointWrap",
            "Sampler_LinearClamp",
            "Sampler_LinearWrap",
            "Sampler_Aniso16xClamp",
            "Sampler_Aniso16xWrap",
            "Sampler_ShadowCompare"
        };

        for (uint32_t i = 0; i < static_cast<uint32_t>(SamplerType::Count); ++i) {
            auto type = static_cast<SamplerType>(i);
            VkSamplerCreateInfo info = GetSamplerCreateInfo(type, maxAniso);

            VkResult res = vkCreateSampler(device->GetHandle(), &info, nullptr, &samplers[i]);
            ENGINE_VERIFY(res == VK_SUCCESS, "VulkanSamplers: Failed to create sampler!");

            device->SetObjectName(samplers[i], samplerNames[i]);
        }
    }

    void VulkanSamplers::DestroySamplers() noexcept
    {
        if (device && device->GetHandle() != VK_NULL_HANDLE) {
            for (VkSampler& sampler : samplers) {
                if (sampler != VK_NULL_HANDLE) {
                    vkDestroySampler(device->GetHandle(), sampler, nullptr);
                    sampler = VK_NULL_HANDLE;
                }
            }
        }
    }

} // namespace Engine