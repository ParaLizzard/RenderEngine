#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <functional>

#include <format>

#include "Core/Hash.h"

namespace Engine {
    // 64-bit compact runtime entity identifier
    class UUID {
    public:
        UUID(); // Generates a random non-zero 64-bit identifier
        explicit UUID(uint64_t uuid) : value(uuid) {}
        UUID(const UUID&) = default;
        UUID& operator=(const UUID&) = default;

        // Conversion from UUID type to uint64_t
        explicit operator uint64_t() const noexcept { return value; }

        ENGINE_NODISCARD uint64_t Get() const noexcept { return value; }
        ENGINE_NODISCARD bool IsValid() const noexcept { return value != 0; }

        ENGINE_NODISCARD std::string ToString() const;

        bool operator==(const UUID& other) const noexcept { return value == other.value; }
        bool operator!=(const UUID& other) const noexcept { return value != other.value; }
        bool operator<(const UUID& other) const noexcept { return value < other.value; }

    private:
        uint64_t value = 0;
    };

    // 128-bit RFC 4122 Version 4 UUID
    struct UUID128 {
        constexpr UUID128() = default;
        constexpr UUID128(uint64_t h, uint64_t l) noexcept : high(h), low(l) {}

        uint64_t high = 0;
        uint64_t low = 0;

        // Generates the UUID
        ENGINE_NODISCARD static UUID128 Generate();
        // Creates UUID from string
        ENGINE_NODISCARD static UUID128 FromString(std::string_view str);
        //Converts it to string with hyphens
        ENGINE_NODISCARD std::string ToString() const;

        ENGINE_NODISCARD bool IsValid() const noexcept {return high != 0 || low != 0; }
        bool operator==(const UUID128& o) const noexcept {return high == o.high && low == o.low; }
        bool operator!=(const UUID128& o) const noexcept {return high != o.high || low != o.low; }
        bool operator<(const UUID128& o) const noexcept {return high < o.high || (high == o.high && low < o.low); }
    };


}

namespace std {
    // Allows you to use UUID and UUID128 in unordered maps and sets
    template<>
    struct hash<Engine::UUID> {
        size_t operator()(const Engine::UUID& uuid) const noexcept {
            return static_cast<size_t>(uuid.Get());
        }
    };

    template<>
    struct hash<Engine::UUID128> {
        size_t operator()(const Engine::UUID128& uuid) const noexcept {
            uint64_t seed = uuid.high;
            Engine::HashCombine(seed, uuid.low);
            return static_cast<size_t>(seed);
        }
    };

    // Lets you format the UUID to a string
    template<>
    struct formatter<Engine::UUID> : std::formatter<std::string> {
        auto format(const Engine::UUID& id, std::format_context& ctx) const {
            return std::formatter<std::string>::format(id.ToString(), ctx);
        }
    };
    template<>
    struct formatter<Engine::UUID128> : std::formatter<std::string> {
        auto format(const Engine::UUID128& id, std::format_context& ctx) const {
            return std::formatter<std::string>::format(id.ToString(), ctx);
        }
    };
}