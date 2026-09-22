#pragma once

#include <cstdint>
#include <type_traits>
#include <format>
#include <functional>

#include "Core/CoreDefines.h"
#include "AssetSystem/AssetMetadata.h"

namespace Engine {

    class Mesh;
    class VulkanTexture;
    struct Material;
    class Shader;

    template<typename T>
    struct AssetTraits {
        static constexpr AssetType Type = AssetType::Unknown;
    };

    template<> struct AssetTraits<void> { static constexpr AssetType Type = AssetType::Unknown; };
    template<> struct AssetTraits<Mesh> { static constexpr AssetType Type = AssetType::Mesh; };
    template<> struct AssetTraits<VulkanTexture> { static constexpr AssetType Type = AssetType::Texture; };
    template<> struct AssetTraits<Material> { static constexpr AssetType Type = AssetType::Material; };
    template<> struct AssetTraits<Shader> { static constexpr AssetType Type = AssetType::Shader; };


    template<typename T = void>
    class AssetHandle {
    public:
        using AssetValueType = T;

        static constexpr uint64_t IndexMask      = 0x00000000FFFFFFFFULL;
        static constexpr uint64_t GenerationMask = 0x0000FFFF00000000ULL;
        static constexpr uint64_t TypeMask       = 0xFFFF000000000000ULL;

        static constexpr uint32_t GenerationShift = 32;
        static constexpr uint32_t TypeShift       = 48;

        constexpr AssetHandle() noexcept : raw(0) {}

        explicit constexpr AssetHandle(uint64_t rawValue) noexcept : raw(rawValue) {}

        static constexpr AssetHandle<T> Create(uint32_t index, uint16_t generation, uint16_t type) noexcept {
            uint64_t packed = (static_cast<uint64_t>(index) & IndexMask)
                            | ((static_cast<uint64_t>(generation) << GenerationShift) & GenerationMask)
                            | ((static_cast<uint64_t>(type) << TypeShift) & TypeMask);
            return AssetHandle<T>(packed);
        }

        static constexpr AssetHandle<T> Create(uint32_t index, uint16_t generation, AssetType type) noexcept {
            return Create(index, generation, static_cast<uint16_t>(type));
        }

        static constexpr AssetHandle<T> Create(uint32_t index, uint16_t generation) noexcept {
            return Create(index, generation, static_cast<uint16_t>(AssetTraits<T>::Type));
        }

        static constexpr AssetHandle<T> FromRaw(uint64_t rawValue) noexcept {
            return AssetHandle<T>(rawValue);
        }

        static constexpr AssetHandle<T> Invalid() noexcept {
            return AssetHandle<T>(0);
        }

        ENGINE_NODISCARD constexpr uint32_t GetIndex() const noexcept {
            return static_cast<uint32_t>(raw & IndexMask);
        }

        ENGINE_NODISCARD constexpr uint16_t GetGeneration() const noexcept {
            return static_cast<uint16_t>((raw & GenerationMask) >> GenerationShift);
        }

        ENGINE_NODISCARD constexpr uint16_t GetTypeID() const noexcept {
            return static_cast<uint16_t>((raw & TypeMask) >> TypeShift);
        }

        ENGINE_NODISCARD constexpr AssetType GetAssetType() const noexcept {
            return static_cast<AssetType>(GetTypeID());
        }

        ENGINE_NODISCARD constexpr uint64_t GetRaw() const noexcept {
            return raw;
        }

        ENGINE_NODISCARD constexpr uint64_t GetID() const noexcept {
            return raw;
        }

        ENGINE_NODISCARD constexpr bool IsValid() const noexcept {
            return raw != 0;
        }

        constexpr explicit operator bool() const noexcept {
            return IsValid();
        }

        ENGINE_NODISCARD constexpr bool MatchesGeneration(uint16_t currentGeneration) const noexcept {
            return GetGeneration() == currentGeneration;
        }

        ENGINE_NODISCARD constexpr bool IsStale(uint16_t currentGeneration) const noexcept {
            return GetGeneration() != currentGeneration;
        }

        ENGINE_NODISCARD bool MatchesMetadata(const AssetMetadata& meta) const noexcept;

        ENGINE_NODISCARD bool IsStale(const AssetMetadata& meta) const noexcept;

        ENGINE_NODISCARD AssetHandle<void> AsUntyped() const noexcept {
            return AssetHandle<void>::FromRaw(raw);
        }

        template<typename U>
        ENGINE_NODISCARD AssetHandle<U> As() const noexcept {
            return AssetHandle<U>::FromRaw(raw);
        }

        explicit operator AssetHandle<void>() const noexcept requires (!std::is_void_v<T>) {
            return AssetHandle<void>::FromRaw(raw);
        }

        constexpr bool operator==(const AssetHandle& other) const noexcept { return raw == other.raw; }
        constexpr bool operator!=(const AssetHandle& other) const noexcept { return raw != other.raw; }
        constexpr bool operator<(const AssetHandle& other) const noexcept { return raw < other.raw; }

    private:
        template<typename U> friend class AssetManager;
        uint64_t raw = 0;
    };

    static_assert(sizeof(AssetHandle<void>) == sizeof(uint64_t), "AssetHandle must be exactly 64 bits!");
    static_assert(std::is_trivially_copyable_v<AssetHandle<void>>, "AssetHandle must be trivially copyable!");

    template<typename T>
    inline bool AssetMetadata::MatchesHandle(const AssetHandle<T>& handle) const noexcept {
        return handle.GetIndex() == index &&
               handle.GetGeneration() == generation &&
               handle.GetTypeID() == static_cast<uint16_t>(type);
    }

    template<typename T>
    inline bool AssetMetadata::IsStale(const AssetHandle<T>& handle) const noexcept {
        return !MatchesHandle(handle);
    }

    template<typename T>
    inline bool AssetHandle<T>::MatchesMetadata(const AssetMetadata& meta) const noexcept {
        return meta.MatchesHandle(*this);
    }

    template<typename T>
    inline bool AssetHandle<T>::IsStale(const AssetMetadata& meta) const noexcept {
        return meta.IsStale(*this);
    }

}

namespace std {
    template<typename T>
    struct hash<Engine::AssetHandle<T>> {
        size_t operator()(const Engine::AssetHandle<T>& handle) const noexcept {
            return std::hash<uint64_t>{}(handle.GetRaw());
        }
    };

    template<typename T>
    struct formatter<Engine::AssetHandle<T>> : std::formatter<std::string> {
        auto format(const Engine::AssetHandle<T>& handle, std::format_context& ctx) const {
            return std::format_to(ctx.out(), "AssetHandle[idx={}, gen={}, type={}]",
                                  handle.GetIndex(), handle.GetGeneration(), handle.GetTypeID());
        }
    };
}