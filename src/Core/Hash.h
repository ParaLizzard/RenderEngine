#pragma once

#include <cstdint>
#include <string_view>
#include <concepts>
#include <functional>

#include "CoreDefines.h"

namespace Engine {
    // Carefully chosen Fowler–Noll–Vo 1a (FNV-1a) prime numbers and their offsets
    namespace Detail {
        inline constexpr uint32_t FNV1A_32_PRIME = 0x01000193u;
        inline constexpr uint32_t FNV1A_32_OFFSET = 0x811C9DC5u;

        inline constexpr uint64_t FNV1A_64_PRIME = 0x00000100000001B3ull;
        inline constexpr uint64_t FNV1A_64_OFFSET = 0xCBF29CE484222325ull;
    }

    // Hashes string into 32-bit hash
    ENGINE_NODISCARD constexpr uint32_t Hash32(std::string_view str) noexcept {
        uint32_t hash = Detail::FNV1A_32_OFFSET;
        for (char c : str) {
            hash ^= static_cast<uint8_t>(c);
            hash *= Detail::FNV1A_32_PRIME;
        }
        return hash;
    }

    // 32-bit raw hashing
    ENGINE_NODISCARD constexpr uint32_t Hash32(const void* data, size_t size) noexcept {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint32_t hash = Detail::FNV1A_32_OFFSET;
        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= Detail::FNV1A_32_PRIME;
        }
        return hash;
    }

    // Hashes string into 64-bit hash
    ENGINE_NODISCARD constexpr uint64_t Hash64(std::string_view str) noexcept {
        uint64_t hash = Detail::FNV1A_64_OFFSET;
        for (char c : str) {
            hash ^= static_cast<uint8_t>(c);
            hash *= Detail::FNV1A_64_PRIME;
        }
        return hash;
    }

    // 64-bit raw hashing
    ENGINE_NODISCARD constexpr uint64_t Hash64(const void* data, size_t size) noexcept {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint64_t hash = Detail::FNV1A_64_OFFSET;
        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= Detail::FNV1A_64_PRIME;
        }
        return hash;
    }

    // Multi 64-bit hash combine
    template<typename T, typename... Rest>
    inline void HashCombine(uint64_t &seed, const T &value, const Rest &... rest) noexcept
    {
        seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        (HashCombine(seed, rest), ...);
    }

    // Combines 64-bit hash
    template<typename T>
    inline void HashCombine(uint64_t& seed, const T& value) noexcept {
        seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }

    // Combines 32-bit hash
    template<typename T>
    inline void HashCombine32(uint32_t& seed, const T& value) noexcept {
        seed ^= static_cast<uint32_t>(std::hash<T>{}(value)) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    }

    // Hashes the function signature
    template<typename T>
    consteval uint64_t TypeID() noexcept
    {

    #if defined(__GNUC__) || defined(__clang__)
        return Hash64(std::string_view(__PRETTY_FUNCTION__));
    #elif defined(_MSC_VER)
        return Hash64(std::string_view(__FUNCSIG__));
    #else
        return 0;
    #endif
    }

    struct TransparentStringHash {
        using is_transparent = void; // C++20 heterogeneous lookup tag
        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
        size_t operator()(const char* s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    template<typename T>
    using TransparentStringMap = std::unordered_map<
        std::string,
        T,
        TransparentStringHash,
        std::equal_to<>
    >;

}


// Hash literals
ENGINE_NODISCARD constexpr uint32_t operator""_hash32(const char* str, size_t len) noexcept {
    return ::Engine::Hash32(std::string_view(str, len));
}

ENGINE_NODISCARD constexpr uint64_t operator""_hash64(const char* str, size_t len) noexcept {
    return ::Engine::Hash64(std::string_view(str, len));
}

ENGINE_NODISCARD constexpr uint64_t operator""_hash(const char* str, size_t len) noexcept {
    return ::Engine::Hash64(std::string_view(str, len));
}

