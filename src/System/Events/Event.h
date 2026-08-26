#pragma once
#include <cstdint>
#include <string>
#include <string_view>

#include "Core/CoreDefines.h"
#include "Core/Hash.h"

namespace Engine {

    using EventTypeID = uint32_t;
    using SubscriptionToken = uint64_t;

    enum class EventCategory : uint32_t {
        None          = 0,
        Application   = 1 << 0,
        Window        = 1 << 1,
        Input         = 1 << 2,
        Keyboard      = 1 << 3,
        Mouse         = 1 << 4,
        MouseButton   = 1 << 5,
        Scene         = 1 << 6,
        Renderer      = 1 << 7,
        Asset         = 1 << 8,
        Editor        = 1 << 9,
        User          = 1 << 10
    };
    ENGINE_ENUM_CLASS_FLAGS(EventCategory);

    class Event {
    public:
        virtual ~Event() = default;

        ENGINE_NODISCARD virtual EventTypeID GetEventType() const noexcept = 0;
        ENGINE_NODISCARD virtual const char* GetName() const noexcept = 0;
        ENGINE_NODISCARD virtual EventCategory GetCategoryFlags() const noexcept = 0;
        ENGINE_NODISCARD virtual std::string ToString() const { return GetName(); }

        ENGINE_NODISCARD bool IsInCategory(EventCategory category) const noexcept {
            return EnumHasAnyFlags(GetCategoryFlags(), category);
        }

        bool handled = false;
    };

    #define EVENT_CLASS_TYPE(TypeName) \
        static constexpr std::string_view GetStaticTypeName() noexcept { return #TypeName; } \
        static constexpr ::Engine::EventTypeID GetStaticType() noexcept { return ::Engine::Hash32(#TypeName); } \
        virtual ::Engine::EventTypeID GetEventType() const noexcept override { return GetStaticType(); } \
        virtual const char* GetName() const noexcept override { return #TypeName; }

    #define EVENT_CLASS_CATEGORY(CategoryFlags) \
        virtual ::Engine::EventCategory GetCategoryFlags() const noexcept override { return CategoryFlags; }

} // namespace Engine
