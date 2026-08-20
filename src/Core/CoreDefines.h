#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

/*
 * Compiler-specific intrinsics
 * forceinline -> Compilers interpret inline as hint causing to some multiplication functions to not inline. It need to be forced
 * noinline -> When there is too much inlining from cold paths it causes to bloat L1 cache for more important code
 * debugbreak -> Invokes hardware breakpoint instruction. Can be conditional. Is faster than IDE breakpoint. Persistent in code.
 */
#if defined(_MSC_VER)
    #define ENGINE_FORCEINLINE __forceinline
    #define ENGINE_NOINLINE __declspec(noinline)
    #define ENGINE_DEBUGBREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
    #define ENGINE_FORCEINLINE inline __attribute__((always_inline))
    #define ENGINE_NOINLINE __attribute__((noinline))
    #define ENGINE_DEBUGBREAK() __builtin_trap()
#else
    #define ENGINE_FORCEINLINE inline
    #define ENGINE_NOINLINE
    #define ENGINE_DEBUGBREAK()
#endif

/*
 * Branch prediction
 */

// Forces compiler to issue warning if caller ignores the return (e.g ignores resource handle thus leaking memory)
#define ENGINE_NODISCARD [[nodiscard]]
// Prevents compiler warnings of unused (e.g In debug blocks #ifdef)
#define ENGINE_MAYBE_UNUSED [[maybe_unused]]
// For paths that are more likely to be executed
#define ENGINE_LIKELY [[likely]]
// For paths that are much less likely to be executed (errors, corruptions)
#define ENGINE_UNLIKELY [[unlikely]]

/*
 * Hardware cache lines
 * Some CPUs fetch memory in 64 bytes block called "Cache lines".
 * If two CPU workers can share cache line which can stall one of them.
 * So by forcing "alignas" we force that each workers data structure starts with fresh boundary.
 */
#define ENGINE_CACHELINE_SIZE 64
#define ENGINE_ALIGN_CACHELINE alignas(ENGINE_CACHELINE_SIZE)

/*
 * RAII ownership
 */

// For classes that must not copy (e.g gpu resources, device)
#define ENGINE_NON_COPYABLE(ClassName) \
ClassName(const ClassName&) = delete; \
ClassName& operator=(const ClassName&) = delete;

// For classes that must not move
#define ENGINE_NON_MOVABLE(ClassName) \
ClassName(ClassName&&) = delete; \
ClassName& operator=(ClassName&&) = delete;

/*
 * Memory size literals (e.q 64_KB)
 */
constexpr size_t operator""_KB(unsigned long long val) noexcept { return val * 1024ULL; }
constexpr size_t operator""_MB(unsigned long long val) noexcept { return val * 1024ULL * 1024ULL; }
constexpr size_t operator""_GB(unsigned long long val) noexcept { return val * 1024ULL * 1024ULL * 1024ULL; }

/*
 * Enum class bitwise flags helpers
 */
template<typename EnumType>
ENGINE_NODISCARD inline constexpr bool EnumHasAnyFlags(EnumType value, EnumType flags) noexcept {
    using Underlying = std::underlying_type_t<EnumType>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) != static_cast<Underlying>(0);
}
template<typename EnumType>
ENGINE_NODISCARD inline constexpr bool EnumHasAllFlags(EnumType value, EnumType flags) noexcept {
    using Underlying = std::underlying_type_t<EnumType>;
    return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) == static_cast<Underlying>(flags);
}

/*
 * Bitwise flags
 * Allows doing bitwise operation with enum flags (EnumHasAnyFlags function for condition)
 */
#define ENGINE_ENUM_CLASS_FLAGS(EnumType) \
ENGINE_NODISCARD inline constexpr EnumType operator|(EnumType a, EnumType b) noexcept { \
using Underlying = std::underlying_type_t<EnumType>; \
return static_cast<EnumType>(static_cast<Underlying>(a) | static_cast<Underlying>(b)); \
} \
ENGINE_NODISCARD inline constexpr EnumType operator&(EnumType a, EnumType b) noexcept { \
using Underlying = std::underlying_type_t<EnumType>; \
return static_cast<EnumType>(static_cast<Underlying>(a) & static_cast<Underlying>(b)); \
} \
ENGINE_NODISCARD inline constexpr EnumType operator^(EnumType a, EnumType b) noexcept { \
using Underlying = std::underlying_type_t<EnumType>; \
return static_cast<EnumType>(static_cast<Underlying>(a) ^ static_cast<Underlying>(b)); \
} \
ENGINE_NODISCARD inline constexpr EnumType operator~(EnumType a) noexcept { \
using Underlying = std::underlying_type_t<EnumType>; \
return static_cast<EnumType>(~static_cast<Underlying>(a)); \
} \
inline constexpr EnumType& operator|=(EnumType& a, EnumType b) noexcept { return a = a | b; } \
inline constexpr EnumType& operator&=(EnumType& a, EnumType b) noexcept { return a = a & b; } \
inline constexpr EnumType& operator^=(EnumType& a, EnumType b) noexcept { return a = a ^ b; }

/*
 * Fast bitwise alignment
 */
template<typename T>
ENGINE_NODISCARD constexpr bool IsPowerOfTwo(T value) noexcept {
    return value > 0 && (value & (value - 1)) == 0;
}
template<typename T>
ENGINE_NODISCARD constexpr T AlignUp(T value, size_t alignment) noexcept {
    return static_cast<T>((value + static_cast<T>(alignment) - 1) & ~static_cast<T>(alignment - 1));
}
template<typename T>
ENGINE_NODISCARD constexpr T AlignDown(T value, size_t alignment) noexcept {
    return static_cast<T>(value & ~static_cast<T>(alignment - 1));
}

