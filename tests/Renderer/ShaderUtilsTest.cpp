#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include "Renderer/ShaderUtils.h"

class ShaderUtilsReadFileTest : public ::testing::Test {
protected:
    std::string tempFilePath;

    void SetUp() override
    {
        // Create a temporary file with known content
        tempFilePath = (std::filesystem::temp_directory_path() / "shader_utils_test.bin").string();
    }

    void TearDown() override
    {
        std::filesystem::remove(tempFilePath);
    }

    void writeTestFile(const std::vector<char> &data)
    {
        std::ofstream file(tempFilePath, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.close();
    }
};

TEST_F(ShaderUtilsReadFileTest, ReadValidFileReturnsExactContents)
{
    std::vector<char> expected = {0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00};
    writeTestFile(expected);

    auto result = Engine::ShaderUtils::readFile(tempFilePath);

    ASSERT_EQ(result.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(result[i], expected[i]) << "Mismatch at byte " << i;
    }
}

TEST_F(ShaderUtilsReadFileTest, ReadEmptyFileReturnsEmptyVector)
{
    std::vector<char> empty;
    writeTestFile(empty);

    auto result = Engine::ShaderUtils::readFile(tempFilePath);
    EXPECT_TRUE(result.empty());
}

TEST_F(ShaderUtilsReadFileTest, ReadLargerFilePreservesAllBytes)
{
    // 1024 bytes with varying content
    std::vector<char> data(1024);
    for (int i = 0; i < 1024; ++i) {
        data[i] = static_cast<char>(i & 0xFF);
    }
    writeTestFile(data);

    auto result = Engine::ShaderUtils::readFile(tempFilePath);
    ASSERT_EQ(result.size(), 1024);
    EXPECT_EQ(result, data);
}

TEST_F(ShaderUtilsReadFileTest, ReadNonexistentFileThrows)
{
    EXPECT_THROW(
        Engine::ShaderUtils::readFile("this_file_does_not_exist_12345.spv"),
        std::runtime_error
    );
}

TEST_F(ShaderUtilsReadFileTest, ReadNonexistentFileErrorContainsFilename)
{
    try {
        Engine::ShaderUtils::readFile("my_missing_shader.spv");
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error &e) {
        EXPECT_NE(std::string(e.what()).find("my_missing_shader.spv"), std::string::npos)
            << "Error message should contain the filename";
    }
}

TEST_F(ShaderUtilsReadFileTest, ReadFileSizeMatchesActualSize)
{
    std::vector<char> data(512, 'A');
    writeTestFile(data);

    auto result = Engine::ShaderUtils::readFile(tempFilePath);
    EXPECT_EQ(result.size(), 512);
}

// Verify SPIR-V magic number can be read back correctly
TEST_F(ShaderUtilsReadFileTest, ReadSpirvMagicNumber)
{
    // SPIR-V magic number: 0x07230203
    std::vector<char> spirvHeader = {0x03, 0x02, 0x23, 0x07};
    writeTestFile(spirvHeader);

    auto result = Engine::ShaderUtils::readFile(tempFilePath);
    ASSERT_EQ(result.size(), 4);

    uint32_t magic;
    std::memcpy(&magic, result.data(), sizeof(uint32_t));
    EXPECT_EQ(magic, 0x07230203u) << "SPIR-V magic number should be preserved";
}
