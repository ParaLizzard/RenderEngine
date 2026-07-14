#pragma once
#include <exception>
#include <string>

namespace Engine
{
    class Exception : public std::exception
    {
    public:
        Exception(int line, const char* file) noexcept;
        const char*  what() const noexcept override;
        virtual const char* getType() const noexcept;
        int GetLine() const noexcept;
        const std::string& getFile() const noexcept;
        std::string getOriginString() const noexcept;
    private:
        int line;
        std::string file;
    protected:
        mutable std::string whatBuffer;
    };
} // Engine
