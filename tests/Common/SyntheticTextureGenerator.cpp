#include "Common/SyntheticTextureGenerator.h"
#include <algorithm>

namespace Engine::Test {

    SyntheticTextureRGBA8 SyntheticTextureGenerator::GenerateCheckerboard(
        uint32_t width,
        uint32_t height,
        uint32_t tileSize,
        glm::u8vec4 colorA,
        glm::u8vec4 colorB)
    {
        SyntheticTextureRGBA8 tex;
        tex.width = width;
        tex.height = height;
        tex.pixels.resize(static_cast<size_t>(width) * height * 4);

        tileSize = std::max(tileSize, 1u);

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                bool isTileA = ((x / tileSize) + (y / tileSize)) % 2 == 0;
                glm::u8vec4 c = isTileA ? colorA : colorB;

                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                tex.pixels[idx + 0] = c.r;
                tex.pixels[idx + 1] = c.g;
                tex.pixels[idx + 2] = c.b;
                tex.pixels[idx + 3] = c.a;
            }
        }

        return tex;
    }

    SyntheticTextureRGBA32F SyntheticTextureGenerator::GenerateGradient(
        uint32_t width,
        uint32_t height,
        glm::vec4 startColor,
        glm::vec4 endColor)
    {
        SyntheticTextureRGBA32F tex;
        tex.width = width;
        tex.height = height;
        tex.pixels.resize(static_cast<size_t>(width) * height * 4);

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                float t = (width > 1) ? (static_cast<float>(x) / static_cast<float>(width - 1)) : 0.0f;
                glm::vec4 c = glm::mix(startColor, endColor, t);

                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                tex.pixels[idx + 0] = c.r;
                tex.pixels[idx + 1] = c.g;
                tex.pixels[idx + 2] = c.b;
                tex.pixels[idx + 3] = c.a;
            }
        }

        return tex;
    }

    SyntheticTextureRGBA8 SyntheticTextureGenerator::GenerateSolidColor(
        uint32_t width,
        uint32_t height,
        glm::u8vec4 color)
    {
        SyntheticTextureRGBA8 tex;
        tex.width = width;
        tex.height = height;
        tex.pixels.resize(static_cast<size_t>(width) * height * 4);

        for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
            tex.pixels[i * 4 + 0] = color.r;
            tex.pixels[i * 4 + 1] = color.g;
            tex.pixels[i * 4 + 2] = color.b;
            tex.pixels[i * 4 + 3] = color.a;
        }

        return tex;
    }

} // namespace Engine::Test
