#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <functional>
#include <vector>
#include <unordered_map>
#include <concepts>
#include <optional>
#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <charconv>

#include "CoreDefines.h"
#include "Log.h"

namespace Engine {
    // Flags for required action or modification
    enum class CVarFlags : uint32_t {
        None            = 0,
        SaveToConfig    = 1 << 0,
        ReadOnly        = 1 << 1,
        RequiresRestart = 1 << 2,
        RenderDirty     = 1 << 3
    };
    ENGINE_ENUM_CLASS_FLAGS(CVarFlags);

    class CVarBase {
    public:
        CVarBase(std::string_view name, std::string_view description, CVarFlags flags);
        virtual ~CVarBase() = default;

        // Get name of the CVar
        ENGINE_NODISCARD std::string_view GetName() const noexcept { return name; }

        // Get description of the CVar
        ENGINE_NODISCARD std::string_view GetDescription() const noexcept { return description; }

        // Get CVarFlags flags of the CVar
        ENGINE_NODISCARD CVarFlags GetFlags() const noexcept { return flags; }

        // If CVar has specific flag
        ENGINE_NODISCARD bool HasFlag(CVarFlags flag) const noexcept {
            return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
        }

        ENGINE_NODISCARD virtual std::string_view GetTypeName() const noexcept = 0;
        ENGINE_NODISCARD virtual bool IsModified() const noexcept = 0;
        ENGINE_NODISCARD virtual std::string ToString() const = 0;
        virtual bool FromString(std::string_view val) = 0;

    protected:
        std::string name;
        std::string description;
        CVarFlags flags;
    };

    template<typename T>
    class CVar : public CVarBase {
    public:
        using Callback = std::function<void(const T& oldVal, const T& newVal)>;

        CVar(std::string_view name,
             const T &defaultValue,
             std::string_view description,
             CVarFlags flags = CVarFlags::None,
             std::optional<T> minVal = std::nullopt,
             std::optional<T> maxVal = std::nullopt):
            CVarBase(name, description, flags), value(defaultValue), defaultValue(defaultValue),
            minValue(minVal), maxValue(maxVal)
        {
            if constexpr (std::is_trivially_copyable_v<T>) {
                atomicValue.store(defaultValue, std::memory_order_relaxed);
            }
        }

        // Get the value of the CVar
        T Get() const noexcept {
            if constexpr (std::is_trivially_copyable_v<T>) {
                return atomicValue.load(std::memory_order_relaxed);
            } else {
                std::shared_lock<std::shared_mutex> lock(rwMutex);
                return value;
            }
        }

        // Set new value of the CVar
        void Set(const T& newValue) {
            if (HasFlag(CVarFlags::ReadOnly)) return;
            T clampedVal = newValue;
            if constexpr (std::is_arithmetic_v<T>) {
                if (minValue.has_value()) clampedVal = std::max(clampedVal, *minValue);
                if (maxValue.has_value()) clampedVal = std::min(clampedVal, *maxValue);
            }
            std::vector<Callback> callbacksToInvoke;
            T oldVal;
            {
                std::unique_lock<std::shared_mutex> lock(rwMutex);
                if (value == clampedVal) return;
                oldVal = value;
                value = clampedVal;
                if constexpr (std::is_trivially_copyable_v<T>) {
                    atomicValue.store(clampedVal, std::memory_order_relaxed);
                }
                callbacksToInvoke.reserve(callbacks.size());
                for (const auto& [token, cb] : callbacks) {
                    callbacksToInvoke.push_back(cb);
                }
            }
            for (const auto& cb : callbacksToInvoke) {
                cb(oldVal, clampedVal);
            }
        }


        using SubscriptionToken = size_t;
        // Add new callback to the CVar when value changes
        SubscriptionToken OnChanged(Callback cb) {
            std::unique_lock<std::shared_mutex> lock(rwMutex);
            SubscriptionToken token = nextToken++;
            callbacks.emplace_back(token, std::move(cb));
            return token;
        }

        // Remove callback from executing after CVar value change
        void RemoveCallback(SubscriptionToken token) {
            std::unique_lock<std::shared_mutex> lock(rwMutex);
            std::erase_if(callbacks, [token](auto& entry) { return entry.first == token; });
        }

        // Convert value to string representation
        std::string ToString() const override {
            T val = Get();
            if constexpr (std::is_same_v<T, bool>) {
                return val ? "true" : "false";
            } else if constexpr (std::is_integral_v<T>) {
                return std::to_string(val);
            } else if constexpr (std::is_floating_point_v<T>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return val;
            } else {
                return "";
            }
        }

        // Set new value from string
        bool FromString(std::string_view val) override {
            if (HasFlag(CVarFlags::ReadOnly)) return false;
            if constexpr (std::is_same_v<T, bool>) {
                if (val == "1" || val == "true" || val == "True" || val == "TRUE" || val == "on" || val == "ON" || val == "true" || val == "yes") {
                    Set(true);
                    return true;
                }
                if (val == "0" || val == "false" || val == "False" || val == "FALSE" || val == "off" || val == "OFF"|| val == "no") {
                    Set(false);
                    return true;
                }
                return false;
            } else if constexpr (std::is_integral_v<T>) {
                T parsed = 0;
                auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), parsed);
                if (ec == std::errc{} && ptr == val.data() + val.size()) {
                    Set(parsed);
                    return true;
                }
                return false;
            } else if constexpr (std::is_floating_point_v<T>) {
                std::string nullTerminatedStr(val);
                char* endPtr = nullptr;
                float parsed = std::strtof(nullTerminatedStr.c_str(), &endPtr);
                if (endPtr != nullTerminatedStr.c_str() && *endPtr == '\0') {
                    Set(static_cast<T>(parsed));
                    return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, std::string>) {
                Set(std::string(val));
                return true;
            } else {
                return false;
            }
        }

        // Get default value
        const T& GetDefault() const noexcept { return defaultValue; }

        // Reset to default value
        void ResetToDefault() { Set(defaultValue); }

        // If is CVar modified (current value != default value)
        bool IsModified() const noexcept override {
            std::shared_lock<std::shared_mutex> lock(rwMutex);
            return value != defaultValue;
        }

        std::string_view GetTypeName() const noexcept override
        {
            if constexpr (std::is_same_v<T, bool>) {
                return "bool";
            } else if constexpr (std::is_integral_v<T>) {
                return "int";
            } else if constexpr (std::is_floating_point_v<T>) {
                return "float";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "string";
            }

            LOG_WARN("CVar", "Unknown type '%s'", typeid(T).name());
            return "Unknown type";
        };

    private:
        T value;
        T defaultValue;
        std::optional<T> minValue;
        std::optional<T> maxValue;

        std::conditional_t<std::is_trivially_copyable_v<T>, std::atomic<T>, uint8_t> atomicValue{};

        std::vector<std::pair<SubscriptionToken, Callback>> callbacks;
        SubscriptionToken nextToken = 1;
        mutable std::shared_mutex rwMutex;
    };

    class CVarSystem {
    public:
        // Return static instance of the CVar system
        static CVarSystem& Get();
        void Clear();

        // Register new CVar
        void Register(CVarBase* cvar);
        CVarBase* Find(std::string_view name);

        // Find exact CVar pointer
        template<typename T>
        CVar<T>* FindExact(std::string_view name) {
            return dynamic_cast<CVar<T>*>(Find(name));
        }

        // Query for CVars to search
        std::vector<CVarBase*> Search(std::string_view query);

        // Get all registered CVars
        std::unordered_map<std::string, CVarBase*> GetAll() const {
            std::shared_lock<std::shared_mutex> lock(mutex);
            return cvars;
        }



    private:
        std::unordered_map<std::string, CVarBase*> cvars;
        mutable std::shared_mutex mutex;
    };
}

inline bool CaseInsensitive(std::string_view a, std::string_view b) {
    return std::lexicographical_compare(
        a.begin(), a.end(),
        b.begin(), b.end(),
        [](unsigned char c1, unsigned char c2) {
            return std::tolower(c1) < std::tolower(c2);
        }
    );
}

#define AUTO_CVAR(VarName, Type, Name, DefaultVal, Description, Flags) \
inline ::Engine::CVar<Type> VarName{Name, DefaultVal, Description, Flags}; \
namespace { \
struct _Register_##VarName { \
_Register_##VarName() { ::Engine::CVarSystem::Get().Register(&VarName); } \
} inline _reg_##VarName; \
}
