#include <gtest/gtest.h>
#include <unordered_set>
#include <unordered_map>
#include <format>
#include <vector>

#include "Core/CoreDefines.h"
#include "Core/Types.h"
#include "Core/Hash.h"
#include "Core/UUID.h"

// Sample enum for testing bitwise flags
enum class TestRenderFlags : uint32_t {
    None        = 0,
    DepthTest   = 1 << 0,
    DepthWrite  = 1 << 1,
    CullBack    = 1 << 2,
    Wireframe   = 1 << 3,
    AlphaBlend  = 1 << 4
};
ENGINE_ENUM_CLASS_FLAGS(TestRenderFlags)

class TypesHashTest : public ::testing::Test {};

// =============================================================================
// 1. Compile-Time and Runtime FNV-1a Hashing Tests
// =============================================================================

TEST_F(TypesHashTest, CompileTimeFNV1a)
{
    // Compile-time evaluation via constexpr
    constexpr auto hash64_val = "TestString"_hash;
    constexpr auto hash64_explicit = "TestString"_hash64;
    constexpr auto hash32_val = "TestString"_hash32;

    static_assert(hash64_val != 0);
    static_assert(hash64_val == hash64_explicit);
    static_assert(hash32_val != 0);

    // Verify runtime matches constexpr
    std::string str = "TestString";
    EXPECT_EQ(Engine::Hash64(str), hash64_val);
    EXPECT_EQ(Engine::Hash32(str), hash32_val);

    // Verify different strings produce different hashes
    constexpr auto diffHash = "AnotherString"_hash;
    static_assert(hash64_val != diffHash);
}

TEST_F(TypesHashTest, HashCombineTests)
{
    uint64_t seed1 = 0;
    Engine::HashCombine(seed1, 42, std::string("test"), 3.14159);
    EXPECT_NE(seed1, 0u);

    uint64_t seed2 = 0;
    Engine::HashCombine(seed2, 42, std::string("test"), 3.14159);
    EXPECT_EQ(seed1, seed2);

    uint64_t seed3 = 0;
    Engine::HashCombine(seed3, 43, std::string("test"), 3.14159);
    EXPECT_NE(seed1, seed3);
}

// =============================================================================
// 2. UUID & UUID128 Uniqueness and Correctness Tests
// =============================================================================

TEST_F(TypesHashTest, UUIDUniqueness)
{
    constexpr size_t count = 10'000;
    std::unordered_set<Engine::UUID> unique64;
    unique64.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        Engine::UUID id;
        EXPECT_TRUE(id.IsValid()) << "UUID generated a zero value at iteration " << i;
        EXPECT_NE(id.Get(), 0u);
        unique64.insert(id);
    }

    EXPECT_EQ(unique64.size(), count) << "Collision detected among 10,000 64-bit UUIDs!";
}

TEST_F(TypesHashTest, UUID128UniquenessAndRFC4122Format)
{
    constexpr size_t count = 10'000;
    std::unordered_set<Engine::UUID128> unique128;
    unique128.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        Engine::UUID128 id = Engine::UUID128::Generate();
        EXPECT_TRUE(id.IsValid());

        // Verify RFC 4122 v4 Version bitmask: high bits 12-15 must be 4
        EXPECT_EQ((id.high & 0x000000000000F000ull), 0x0000000000004000ull);

        // Verify RFC 4122 v4 Variant bitmask: top 2 bits of low must be 0b10 (0x8..0xB)
        EXPECT_EQ((id.low & 0xC000000000000000ull), 0x8000000000000000ull);

        unique128.insert(id);
    }

    EXPECT_EQ(unique128.size(), count) << "Collision detected among 10,000 128-bit UUIDs!";
}

TEST_F(TypesHashTest, UUID128StringRoundtrip)
{
    for (int i = 0; i < 100; ++i) {
        Engine::UUID128 original = Engine::UUID128::Generate();
        std::string str = original.ToString();
        EXPECT_EQ(str.size(), 36u);
        EXPECT_EQ(str[8], '-');
        EXPECT_EQ(str[13], '-');
        EXPECT_EQ(str[14], '4'); // Version 4 marker
        EXPECT_EQ(str[18], '-');
        EXPECT_EQ(str[23], '-');

        Engine::UUID128 parsed = Engine::UUID128::FromString(str);
        EXPECT_TRUE(parsed.IsValid());
        EXPECT_EQ(original, parsed);
    }
}

TEST_F(TypesHashTest, UUID128RawHexString)
{
    // Test parsing 32-character raw hex (no hyphens)
    Engine::UUID128 original = Engine::UUID128::Generate();
    std::string formatted = original.ToString();
    
    // Strip hyphens
    std::string rawHex;
    for (char c : formatted) {
        if (c != '-') rawHex += c;
    }
    ASSERT_EQ(rawHex.size(), 32u);

    Engine::UUID128 parsed = Engine::UUID128::FromString(rawHex);
    EXPECT_TRUE(parsed.IsValid());
    EXPECT_EQ(original, parsed);
}

TEST_F(TypesHashTest, UUID128InvalidStrings)
{
    EXPECT_FALSE(Engine::UUID128::FromString("").IsValid());
    EXPECT_FALSE(Engine::UUID128::FromString("short").IsValid());
    EXPECT_FALSE(Engine::UUID128::FromString("f47ac10b-58cc-4372-a567-0e02b2c3d47Z").IsValid()); // 'Z' is invalid hex
    EXPECT_FALSE(Engine::UUID128::FromString("------------------------------------").IsValid());
}

TEST_F(TypesHashTest, UUIDFormatting)
{
    Engine::UUID id(0x1a2b3c4d5e6f7081ull);
    EXPECT_EQ(id.ToString(), "1a2b3c4d5e6f7081");
    EXPECT_EQ(std::format("{}", id), "1a2b3c4d5e6f7081");

    Engine::UUID128 id128(0x0123456789abcdefull, 0xfedcba9876543210ull);
    EXPECT_EQ(id128.ToString(), "01234567-89ab-cdef-fedc-ba9876543210");
    EXPECT_EQ(std::format("{}", id128), "01234567-89ab-cdef-fedc-ba9876543210");
}

// =============================================================================
// 3. Memory Literals and Alignment Tests
// =============================================================================

TEST_F(TypesHashTest, MemoryLiteralsAndAlignment)
{
    static_assert(1_KB == 1024ull);
    static_assert(64_MB == 64ull * 1024ull * 1024ull);
    static_assert(2_GB == 2ull * 1024ull * 1024ull * 1024ull);

    EXPECT_EQ(64_MB, 64 * 1024 * 1024);
    EXPECT_EQ(AlignUp(37u, 16), 48u);
    EXPECT_EQ(AlignDown(37u, 16), 32u);
    EXPECT_EQ(AlignUp(32u, 16), 32u);
    EXPECT_EQ(AlignDown(32u, 16), 32u);

    EXPECT_TRUE(IsPowerOfTwo(1));
    EXPECT_TRUE(IsPowerOfTwo(2));
    EXPECT_TRUE(IsPowerOfTwo(64));
    EXPECT_TRUE(IsPowerOfTwo(1024));
    EXPECT_FALSE(IsPowerOfTwo(0));
    EXPECT_FALSE(IsPowerOfTwo(3));
    EXPECT_FALSE(IsPowerOfTwo(37));
}

// =============================================================================
// 4. Enum Class Bitwise Flags Tests
// =============================================================================

TEST_F(TypesHashTest, EnumClassBitwiseFlags)
{
    TestRenderFlags flags = TestRenderFlags::DepthTest | TestRenderFlags::CullBack;

    EXPECT_TRUE(EnumHasAnyFlags(flags, TestRenderFlags::DepthTest));
    EXPECT_TRUE(EnumHasAnyFlags(flags, TestRenderFlags::CullBack));
    EXPECT_FALSE(EnumHasAnyFlags(flags, TestRenderFlags::Wireframe));

    EXPECT_FALSE(EnumHasAllFlags(flags, TestRenderFlags::DepthTest | TestRenderFlags::Wireframe));
    EXPECT_TRUE(EnumHasAllFlags(flags, TestRenderFlags::DepthTest | TestRenderFlags::CullBack));

    flags |= TestRenderFlags::Wireframe;
    EXPECT_TRUE(EnumHasAnyFlags(flags, TestRenderFlags::Wireframe));

    flags &= ~TestRenderFlags::CullBack;
    EXPECT_FALSE(EnumHasAnyFlags(flags, TestRenderFlags::CullBack));

    flags ^= TestRenderFlags::AlphaBlend;
    EXPECT_TRUE(EnumHasAnyFlags(flags, TestRenderFlags::AlphaBlend));
    flags ^= TestRenderFlags::AlphaBlend;
    EXPECT_FALSE(EnumHasAnyFlags(flags, TestRenderFlags::AlphaBlend));
}

// =============================================================================
// 5. Geometry Types Tests
// =============================================================================

TEST_F(TypesHashTest, ExtentAndRectTests)
{
    Engine::Extent2D ext{ 1920, 1080 };
    EXPECT_FLOAT_EQ(ext.AspectRatio(), 1920.0f / 1080.0f);
    EXPECT_EQ(ext.Area(), 1920ull * 1080ull);
    EXPECT_FALSE(ext.IsEmpty());

    Engine::Extent2D emptyExt{ 0, 100 };
    EXPECT_TRUE(emptyExt.IsEmpty());

    Engine::Rect2D rect{ { 10, 20 }, { 100, 50 } };
    EXPECT_TRUE(rect.Contains(10, 20));
    EXPECT_TRUE(rect.Contains(50, 40));
    EXPECT_TRUE(rect.Contains(109, 69));
    EXPECT_FALSE(rect.Contains(9, 20));
    EXPECT_FALSE(rect.Contains(110, 50));
    EXPECT_FALSE(rect.Contains(50, 70));
}
