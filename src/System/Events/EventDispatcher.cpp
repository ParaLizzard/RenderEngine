#include "EventDispatcher.h"

namespace Engine {
    EventDispatcher &EventDispatcher::Get()
    {
        static EventDispatcher instance;
        return instance;
    }
} // namespace Engine
