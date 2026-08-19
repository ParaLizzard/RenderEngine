#pragma once
#include <cstdint>

enum AAMethod
{
    NONE,
    FXAA,
    TAA
};

enum TonemapMethod
{
    ACES,
    AGX
};

namespace Engine::Config {
    inline constexpr std::uint32_t MAX_SCENE_OBJECTS = 8'000;
    inline constexpr uint32_t MAX_TEXTURES = 4096;
    inline constexpr size_t MAX_FRAMES_IN_FLIGHT = 3;
    inline constexpr bool USE_D16_SHADOW_MAPS = true;
    inline constexpr std::uint32_t CULL_WORKGROUP_SIZE = 256;
    inline constexpr std::uint32_t ENABLE_PCSS = 0;
    inline constexpr std::uint32_t PCF_SAMPLES_CASCADE_0 = 8;
    inline constexpr std::uint32_t PCF_SAMPLES_CASCADE_1 = 4;
    inline constexpr std::uint32_t PCF_SAMPLES_CASCADE_2 = 4;

    // Meshlet configs
    inline constexpr std::uint32_t MAX_VERTICES = 64;
    inline constexpr std::uint32_t MAX_TRIANGLES = 124;
    inline constexpr float CONE_WEIGHT = 0.5;

    // SSAO configs
    inline constexpr float SSAO_STRENGTH = 1.2f;
    inline constexpr float MATERIAL_MAX_ANISOTROPY = 4.0f;

    // AA
    inline constexpr AAMethod CURRENT_AA_METHOD = TAA;
    inline constexpr float TAA_MODULATION_FACTOR = 0.9f;

    inline constexpr TonemapMethod CURRENT_TONEMAP_METHOD = AGX;
    inline constexpr float AGX_EXPOSURE = 2.0f;
    inline constexpr bool AGX_PUNCHY = true;


} // namespace Engine::Config


