#pragma once
#include <cstdint>
#include <cstddef>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include "CoreDefines.h"

namespace Engine {
    // Ints
    using uint8  = uint8_t;
    using uint16 = uint16_t;
    using uint32 = uint32_t;
    using uint64 = uint64_t;

    using int8   = int8_t;
    using int16  = int16_t;
    using int32  = int32_t;
    using int64  = int64_t;

    // Floats
    using float32 = float;
    using float64 = double;

    // Byte
    using byte = uint8_t;

    // Vectors
    using Vec2  = glm::vec2;
    using Vec3  = glm::vec3;
    using Vec4  = glm::vec4;
    using IVec2 = glm::ivec2;
    using IVec3 = glm::ivec3;
    using IVec4 = glm::ivec4;
    using UVec2 = glm::uvec2;
    using UVec3 = glm::uvec3;
    using UVec4 = glm::uvec4;

    // Matrices
    using Mat3 = glm::mat3;
    using Mat4 = glm::mat4;
    using Quat = glm::quat;


    struct Extent2D {
        uint32_t width = 0;
        uint32_t height = 0;
        bool operator==(const Extent2D&) const noexcept = default;

        ENGINE_NODISCARD constexpr float AspectRatio() const noexcept {
            return height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        }
        ENGINE_NODISCARD constexpr uint64_t Area() const noexcept {
            return static_cast<uint64_t>(width) * height;
        }
        ENGINE_NODISCARD constexpr bool IsEmpty() const noexcept {
            return width == 0 || height == 0;
        }
    };

    struct Extent3D {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;

        bool operator==(const Extent3D&) const noexcept = default;
    };

    struct Offset2D {
        int32_t x = 0;
        int32_t y = 0;

        bool operator==(const Offset2D&) const noexcept = default;
    };

    struct Rect2D {
        Offset2D offset;
        Extent2D extent;

        bool operator==(const Rect2D&) const noexcept = default;

        ENGINE_NODISCARD constexpr bool Contains(int32_t px, int32_t py) const noexcept {
            return px >= offset.x && px < (offset.x + static_cast<int32_t>(extent.width)) &&
                   py >= offset.y && py < (offset.y + static_cast<int32_t>(extent.height));
        }
    };

    using DeviceAddress = uint64_t;
    using ResourceIndex = uint32_t;
    inline constexpr ResourceIndex INVALID_RESOURCE_INDEX = ~0u;
}
