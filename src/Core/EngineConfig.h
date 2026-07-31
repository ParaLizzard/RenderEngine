#pragma once
#include <cstdint>

namespace Engine::Config {
    inline constexpr std::uint32_t MAX_SCENE_OBJECTS = 100'000;
    inline constexpr uint32_t MAX_TEXTURES = 4096;
    inline constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;
    inline constexpr bool USE_D16_SHADOW_MAPS = true;
    inline constexpr std::uint32_t CULL_WORKGROUP_SIZE = 256;
    inline constexpr std::uint32_t ENABLE_PCSS = 0;
} // namespace Engine::Config
