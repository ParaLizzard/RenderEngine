#include <gtest/gtest.h>
#include <string>
#include <format>
#include <source_location>

#include "Core/Log.h"
#include "Core/UUID.h"

class LogTest : public ::testing::Test {};

// Helper function to test source location capture
static std::source_location CaptureTestLocation() {
    return std::source_location::current();
}

// =============================================================================
// 1. Format String Compile Check & Basic Formatting
// =============================================================================

TEST_F(LogTest, FormatStringCompileCheck)
{
    // Verify basic types format without exceptions
    EXPECT_NO_THROW({
        LOG_INFO("Core", "Integer value: {}", 42);
        LOG_INFO("Core", "Floating point: {:.2f}", 3.14159f);
        LOG_INFO("Core", "String: {}", std::string("RenderEngine"));
        LOG_INFO("Core", "Multiple args: {} + {} = {}", 2, 2, 4);
    });
}

TEST_F(LogTest, FormatLogWithAllSeverities)
{
    EXPECT_NO_THROW({
        LOG_TRACE("TestCategory", "Trace message with id {}", 100);
        LOG_DEBUG("TestCategory", "Debug message with value {}", 3.14);
        LOG_INFO("TestCategory", "Informational startup message");
        LOG_WARN("TestCategory", "Warning message about asset {}", "texture.png");
        LOG_ERROR("TestCategory", "Error message code {}", -1);
        LOG_FATAL("TestCategory", "Fatal condition logged");
    });
}

// =============================================================================
// 2. Source Location Capture Verification
// =============================================================================

TEST_F(LogTest, SourceLocationCapture)
{
    auto loc = CaptureTestLocation();
    
    // Verify that std::source_location accurately captures the file and function
    EXPECT_NE(loc.file_name(), nullptr);
    EXPECT_GT(loc.line(), 0u);
    EXPECT_NE(loc.function_name(), nullptr);
    EXPECT_TRUE(std::string_view(loc.file_name()).ends_with("LogTest.cpp"));
    EXPECT_NE(std::string_view(loc.function_name()).find("CaptureTestLocation"), std::string_view::npos);
}

// =============================================================================
// 3. User Defined Type Formatting in Logs (UUID / UUID128)
// =============================================================================

TEST_F(LogTest, UserDefinedTypeFormatting)
{
    Engine::UUID entityId(0xDEADBEEF12345678ull);
    Engine::UUID128 assetId(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);

    // Verify std::formatter allows UUIDs to be passed directly to LOG macros
    EXPECT_NO_THROW({
        LOG_INFO("Scene", "Spawned Entity: {}", entityId);
        LOG_INFO("Asset", "Loaded Asset: {}", assetId);
    });
}
