
#include "Exception.h"

#include <sstream>

namespace Engine
{
    Exception::Exception(int line, const char *file) noexcept:
    line(line), file(file)
    {}

    const char * Exception::what() const noexcept
    {
        std::ostringstream oss;
        oss << getType() << std::endl << getOriginString();
        whatBuffer = oss.str();
        return whatBuffer.c_str();
    }

    const char * Exception::getType() const noexcept
    {
        return "Engine::Exception";
    }

    int Exception::GetLine() const noexcept
    {
        return line;
    }

    const std::string & Exception::getFile() const noexcept
    {
        return file;
    }

    std::string Exception::getOriginString() const noexcept
    {
        std::ostringstream oss;
        oss << "[File] " << file << std::endl << "[Line] " << line;
        return oss.str();
    }
} // Engine