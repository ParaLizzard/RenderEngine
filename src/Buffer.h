#pragma once

namespace Engine
{
    class Buffer
    {
    public:
        Buffer();
        ~Buffer();

        Buffer(Buffer const&) = delete;
        Buffer& operator=(Buffer const&) = delete;

    };
}
//RENDERENGINE_BUFFER_H