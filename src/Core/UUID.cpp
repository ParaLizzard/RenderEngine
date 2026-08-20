//
// Created by Jan Varga on 19.08.2026.
//

#include "UUID.h"

#include <random>
#include <optional>
#include "Core/Log.h"

namespace Engine
{
    UUID::UUID()
    {
        thread_local std::mt19937_64 generator{ std::random_device{}() };

        thread_local std::uniform_int_distribution<uint64_t> dis(
            1,
            std::numeric_limits<std::uint64_t>::max()
            );

        value = dis(generator);
    }

    std::string UUID::ToString() const
    {
        return std::format("{:016x}", value);
    }

    UUID128 UUID128::Generate()
    {
        UUID128 id;

        thread_local std::mt19937_64 generator{ std::random_device{}() };

        thread_local std::uniform_int_distribution<uint64_t> dis(
            1,
            std::numeric_limits<std::uint64_t>::max()
            );

        id.low = dis(generator);
        id.high = dis(generator);

        id.high = (id.high & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull;
        id.low = (id.low & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;

        return id;
    }

    constexpr std::optional<uint8_t> HexCharToNibble(char c) noexcept {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        return std::nullopt;
    }

    UUID128 UUID128::FromString(std::string_view str)
    {
        UUID128 id{};
        if (str.size() != 36 && str.size() != 32) {
            LOG_WARN("UUID", "Invalid UUID string");
            return UUID128{};
        }

        size_t hexCount = 0;
        for (char c : str) {
            if (c == '-') {
                continue;
            }

            auto nibbleOpt = HexCharToNibble(c);
            if (!nibbleOpt) {
                LOG_WARN("UUID", "Invalid non-hex character in string");
                return UUID128{}; // Invalid
            }

            if (hexCount < 16) {
                id.high = (id.high << 4) | *nibbleOpt;
            } else {
                id.low = (id.low << 4) | *nibbleOpt;
            }
            hexCount++;
        }

        if (hexCount != 32) {
            LOG_WARN("UUID", "Incomplete or malformed hex string");
            return UUID128{}; // Incomplete or malformed UUID
        }

        return id;
    }

    std::string UUID128::ToString() const
    {
        if (!IsValid()) {
            LOG_WARN("UUID", "Converting invalid UUID128 to string");
            return "";
        }

        // Gets zeropadded 16 hex string numbers
        std::string strLow = std::format("{:016x}", low);
        std::string strHigh = std::format("{:016x}", high);

        // Formats the UUID in 8-4-4-4-12
        return std::format("{}-{}-{}-{}-{}",
        strHigh.substr(0, 8), strHigh.substr(8, 4), strHigh.substr(12, 4),
        strLow.substr(0, 4), strLow.substr(4, 12)
    );
    }
} // Engine