#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <span>
#include <cstdint>

#include "Core/CoreDefines.h"

namespace Engine {
    class VulkanDevice;

    enum class SamplerType : uint32_t {
        PointClamp = 0,
        PointWrap,
        LinearClamp,
        LinearWrap,
        Aniso16xClamp,
        Aniso16xWrap,
        ShadowCompare,
        Count
    };

    class VulkanSamplers {
    public:
        explicit VulkanSamplers(const VulkanDevice& device);
        ~VulkanSamplers();

        VulkanSamplers(const VulkanSamplers&) = delete;
        VulkanSamplers& operator=(const VulkanSamplers&) = delete;
        VulkanSamplers(VulkanSamplers&& other) noexcept;
        VulkanSamplers& operator=(VulkanSamplers&& other) noexcept;

        ENGINE_NODISCARD VkSampler GetSampler(SamplerType type) const noexcept;
        ENGINE_NODISCARD const std::array<VkSampler, static_cast<size_t>(SamplerType::Count)>& GetImmutableSamplers() const noexcept {
            return samplers;
        }

        static VkSamplerCreateInfo GetSamplerCreateInfo(SamplerType type, float maxAnisotropy = 16.0f);

    private:
        void CreateSamplers();
        void DestroySamplers() noexcept;

        const VulkanDevice* device = nullptr;
        std::array<VkSampler, static_cast<size_t>(SamplerType::Count)> samplers{};
    };
} // namespace Engine