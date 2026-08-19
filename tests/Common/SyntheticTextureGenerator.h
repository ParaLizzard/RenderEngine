#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Engine::Test {

    struct SyntheticTextureRGBA8 {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels; // 4 bytes per pixel (RGBA)

        glm::u8vec4 GetPixel(uint32_t x, uint32_t y) const {
            if (x >= width || y >= height) return { 0, 0, 0, 0 };
            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            return { pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3] };
        }
    };

    struct SyntheticTextureRGBA32F {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<float> pixels; // 4 floats per pixel (RGBA)

        glm::vec4 GetPixel(uint32_t x, uint32_t y) const {
            if (x >= width || y >= height) return { 0.0f, 0.0f, 0.0f, 0.0f };
            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            return { pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3] };
        }
    };

    class SyntheticTextureGenerator {
    public:
        // Generates an in-memory RGBA8 checkerboard texture
        static SyntheticTextureRGBA8 GenerateCheckerboard(
            uint32_t width = 64,
            uint32_t height = 64,
            uint32_t tileSize = 8,
            glm::u8vec4 colorA = { 255, 255, 255, 255 },
            glm::u8vec4 colorB = { 0, 0, 0, 255 });

        // Generates an in-memory RGBA32F linear horizontal gradient
        static SyntheticTextureRGBA32F GenerateGradient(
            uint32_t width = 64,
            uint32_t height = 64,
            glm::vec4 startColor = { 1.0f, 0.0f, 0.0f, 1.0f },
            glm::vec4 endColor = { 0.0f, 0.0f, 1.0f, 1.0f });

        // Generates an in-memory RGBA8 solid color texture
        static SyntheticTextureRGBA8 GenerateSolidColor(
            uint32_t width = 64,
            uint32_t height = 64,
            glm::u8vec4 color = { 255, 255, 255, 255 });
    };

} // namespace Engine::Test
