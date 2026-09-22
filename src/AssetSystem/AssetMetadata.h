#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <chrono>

#include "Core/CoreDefines.h"
#include "Core/UUID.h"

namespace Engine {

    template<typename T>
    class AssetHandle;

    enum class AssetType : uint16_t {
        Unknown = 0,
        Mesh = 1,
        Texture = 2,
        Material = 3,
        Shader = 4,
        EnvironmentMap = 5,
        Scene = 6,
        Custom = 7
    };

    ENGINE_NODISCARD inline constexpr const char* AssetTypeToString(AssetType type) noexcept {
        switch (type) {
            case AssetType::Mesh: return "Mesh";
            case AssetType::Texture: return "Texture";
            case AssetType::Material: return "Material";
            case AssetType::Shader: return "Shader";
            case AssetType::EnvironmentMap: return "EnvironmentMap";
            case AssetType::Scene: return "Scene";
            case AssetType::Custom: return "Custom";
            default: return "Unknown";
        }
    }

    enum class AssetState : uint8_t {
        Unloaded = 0,
        Loading = 1,
        Ready = 2,
        Failed = 3
    };

    ENGINE_NODISCARD inline constexpr const char* AssetStateToString(AssetState state) noexcept {
        switch (state) {
            case AssetState::Unloaded: return "Unloaded";
            case AssetState::Loading: return "Loading";
            case AssetState::Ready: return "Ready";
            case AssetState::Failed: return "Failed";
            default: return "Unknown";
        }
    }

    struct AssetMetadata {
        uint32_t index = 0;
        uint16_t generation = 1;
        AssetType type = AssetType::Unknown;
        AssetState state = AssetState::Unloaded;

        UUID guid;

        std::filesystem::path filePath;
        std::string name;
        uint64_t fileSize = 0;
        std::filesystem::file_time_type lastModified{};

        uint64_t cpuMemoryBytes = 0;
        uint64_t gpuMemoryBytes = 0;

        uint32_t referenceCount = 0;
        bool isEngineInternal = false;
        std::vector<uint64_t> dependencies;

        ENGINE_NODISCARD bool IsReady() const noexcept { return state == AssetState::Ready; }
        ENGINE_NODISCARD bool IsLoading() const noexcept { return state == AssetState::Loading; }
        ENGINE_NODISCARD bool IsFailed() const noexcept { return state == AssetState::Failed; }
        ENGINE_NODISCARD bool IsValid() const noexcept {
            return type != AssetType::Unknown && (guid.IsValid() || !filePath.empty());
        }

        template<typename T>
        ENGINE_NODISCARD bool MatchesHandle(const AssetHandle<T>& handle) const noexcept;

        template<typename T>
        ENGINE_NODISCARD bool IsStale(const AssetHandle<T>& handle) const noexcept;
    };

} // namespace Engine