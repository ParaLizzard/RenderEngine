#pragma once

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <cstdint>
#include <cmath>

namespace Engine::TestUtils {

    // =========================================================================
    // Floating-Point & GLM Fuzzy Comparisons
    // =========================================================================

    inline void ExpectNearVec2(const glm::vec2& a, const glm::vec2& b, float eps = 1e-4f) {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
    }

    inline void ExpectNearVec3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    inline void ExpectNearVec4(const glm::vec4& a, const glm::vec4& b, float eps = 1e-4f) {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
        EXPECT_NEAR(a.w, b.w, eps);
    }

    inline void ExpectNearMat4(const glm::mat4& a, const glm::mat4& b, float eps = 1e-4f) {
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                EXPECT_NEAR(a[col][row], b[col][row], eps)
                    << "Mismatch at mat4[" << col << "][" << row << "]";
            }
        }
    }

    inline void ExpectNearQuat(const glm::quat& a, const glm::quat& b, float eps = 1e-4f) {
        // Quaternions q and -q represent the same rotation, check both
        float dotVal = glm::abs(glm::dot(a, b));
        EXPECT_NEAR(dotVal, 1.0f, eps);
    }

    // =========================================================================
    // Memory & Pointer Alignment Validation
    // =========================================================================

    template<typename T>
    inline bool IsAligned(T value, size_t alignment) noexcept {
        if (alignment == 0) return true;
        return (reinterpret_cast<uintptr_t>(value) % alignment) == 0;
    }

    inline bool IsOffsetAligned(uint64_t offset, size_t alignment) noexcept {
        if (alignment == 0) return true;
        return (offset % alignment) == 0;
    }

    // =========================================================================
    // RAII Temporary Directory Sandbox for File I/O Tests
    // =========================================================================

    class ScopedTempDir {
    public:
        ScopedTempDir(std::string_view prefix = "engine_test_temp_") {
            auto tempRoot = std::filesystem::temp_directory_path();
            path = tempRoot / (std::string(prefix) + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(path);
        }

        ~ScopedTempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        const std::filesystem::path& GetPath() const noexcept { return path; }
        std::string GetPathString() const { return path.string(); }

        std::filesystem::path FilePath(std::string_view filename) const {
            return path / filename;
        }

    private:
        std::filesystem::path path;
    };

} // namespace Engine::TestUtils
