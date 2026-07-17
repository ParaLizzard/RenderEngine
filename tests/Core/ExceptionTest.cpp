#include <gtest/gtest.h>
#include "Core/Exception.h"
#include <string>

class ExceptionTest : public ::testing::Test {};

TEST_F(ExceptionTest, StoresLineNumber)
{
    Engine::Exception ex(42, "test_file.cpp");
    EXPECT_EQ(ex.GetLine(), 42);
}

TEST_F(ExceptionTest, StoresFileName)
{
    Engine::Exception ex(10, "src/Core/SomeFile.cpp");
    EXPECT_EQ(ex.getFile(), "src/Core/SomeFile.cpp");
}

TEST_F(ExceptionTest, GetTypeReturnsEngineException)
{
    Engine::Exception ex(1, "file.cpp");
    EXPECT_STREQ(ex.getType(), "Engine::Exception");
}

TEST_F(ExceptionTest, WhatContainsTypeName)
{
    Engine::Exception ex(1, "file.cpp");
    std::string msg = ex.what();
    EXPECT_NE(msg.find("Engine::Exception"), std::string::npos)
        << "what() should contain the type name";
}

TEST_F(ExceptionTest, WhatContainsOriginString)
{
    Engine::Exception ex(99, "src/MyFile.h");
    std::string msg = ex.what();

    EXPECT_NE(msg.find("[File]"), std::string::npos)
        << "what() should contain [File] tag";
    EXPECT_NE(msg.find("[Line]"), std::string::npos)
        << "what() should contain [Line] tag";
}

TEST_F(ExceptionTest, GetOriginStringContainsFileAndLine)
{
    Engine::Exception ex(123, "path/to/file.cpp");
    std::string origin = ex.getOriginString();

    EXPECT_NE(origin.find("path/to/file.cpp"), std::string::npos)
        << "Origin should contain the file path";
    EXPECT_NE(origin.find("123"), std::string::npos)
        << "Origin should contain the line number";
}

TEST_F(ExceptionTest, GetOriginStringFormat)
{
    Engine::Exception ex(50, "myfile.cpp");
    std::string origin = ex.getOriginString();

    EXPECT_NE(origin.find("[File] myfile.cpp"), std::string::npos);
    EXPECT_NE(origin.find("[Line] 50"), std::string::npos);
}

TEST_F(ExceptionTest, IsSubclassOfStdException)
{
    Engine::Exception ex(1, "file.cpp");
    const std::exception &base = ex;
    EXPECT_NE(base.what(), nullptr);
}

TEST_F(ExceptionTest, LineZeroIsValid)
{
    Engine::Exception ex(0, "file.cpp");
    EXPECT_EQ(ex.GetLine(), 0);
}

TEST_F(ExceptionTest, EmptyFilenameIsValid)
{
    Engine::Exception ex(1, "");
    EXPECT_EQ(ex.getFile(), "");
}

TEST_F(ExceptionTest, LargeLineNumber)
{
    Engine::Exception ex(999999, "file.cpp");
    EXPECT_EQ(ex.GetLine(), 999999);
    EXPECT_NE(ex.getOriginString().find("999999"), std::string::npos);
}

TEST_F(ExceptionTest, WhatIsRepeatable)
{
    Engine::Exception ex(10, "file.cpp");

    // Call what() twice — should be consistent
    std::string first = ex.what();
    std::string second = ex.what();
    EXPECT_EQ(first, second);
}
