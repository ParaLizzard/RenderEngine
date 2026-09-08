#pragma once
#include "Core/Log.h"
#include "Core/CoreDefines.h"

#ifndef NDEBUG
    #define ENGINE_ASSERT(Condition, Fmt, ...) \
    do { \
    if (!(Condition)) { \
    LOG_FATAL("Assert", "Assertion failed: ({}) in {}:{} - " Fmt, #Condition, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    ENGINE_DEBUGBREAK(); \
    } \
    } while (false)
#else
    #define ENGINE_ASSERT(Condition, Fmt, ...) do { (void)(Condition); } while(false)
#endif

#define ENGINE_VERIFY(Condition, Fmt, ...) \
do { \
if (!(Condition)) { \
LOG_FATAL("Verify", "Verification failed: ({}) in {}:{} - " Fmt, #Condition, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__); \
ENGINE_DEBUGBREAK(); \
} \
} while (false)
