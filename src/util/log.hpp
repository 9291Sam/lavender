#pragma once

#include "misc.hpp"
#include <chrono>
#include <cstdint>
#include <format>
#include <source_location>
#include <string>
#include <utility>

namespace util
{
    enum class LoggingLevel : std::uint8_t
    {
        Trace = 0,
        Debug = 1,
        Log   = 2,
        Warn  = 3,
        Fatal = 4,
        Panic = 5,
    };
    const char* getStringOfLoggingLevel(LoggingLevel);

    void installGlobalLoggerRacy();
    void removeGlobalLoggerRacy();

    void         setLoggingLevel(LoggingLevel);
    LoggingLevel getCurrentLevel();

    void asynchronouslyLog(
        std::string                           message,
        LoggingLevel                          level,
        std::source_location                  location,
        std::chrono::system_clock::time_point time);

#define MAKE_LOGGER(LEVEL) /* NOLINT */                                        \
    template<class... Ts>                                                      \
    struct log##LEVEL /* NOLINT */                                             \
    {                                                                          \
        log##LEVEL(/* NOLINT*/                                                 \
                   std::format_string<Ts...> fmt,                              \
                   Ts&&... args,                                               \
                   const std::source_location& location =                      \
                       std::source_location::current())                        \
        {                                                                      \
            using enum LoggingLevel;                                           \
            if (util::toUnderlying(getCurrentLevel())                          \
                <= util::toUnderlying(LEVEL))                                  \
            {                                                                  \
                asynchronouslyLog(                                             \
                    std::format(fmt, std::forward<Ts>(args)...),               \
                    LEVEL,                                                     \
                    location,                                                  \
                    std::chrono::system_clock::now());                         \
            }                                                                  \
        }                                                                      \
    };                                                                         \
    template<class... Ts>                                                      \
    log##LEVEL(std::format_string<Ts...>, Ts&&...)->log##LEVEL<Ts...>;

    MAKE_LOGGER(Trace)
    MAKE_LOGGER(Debug)
    MAKE_LOGGER(Log)
    MAKE_LOGGER(Warn)
    MAKE_LOGGER(Fatal)

#undef MAKE_LOGGER

#define MAKE_ASSERT(LEVEL, THROW_ON_FAIL) /* NOLINT */                         \
    template<class... Ts>                                                      \
    struct assert##LEVEL                                                       \
    {                                                                          \
        assert##LEVEL(                                                         \
            bool                      condition,                               \
            std::format_string<Ts...> fmt,                                     \
            Ts&&... args,                                                      \
            const std::source_location& location =                             \
                std::source_location::current())                               \
        {                                                                      \
            using enum LoggingLevel;                                           \
            if (!condition                                                     \
                && util::toUnderlying(getCurrentLevel())                       \
                       <= util::toUnderlying(LEVEL)) [[unlikely]]              \
            {                                                                  \
                if constexpr (THROW_ON_FAIL)                                   \
                {                                                              \
                    std::string message =                                      \
                        std::format(fmt, std::forward<Ts>(args)...);           \
                                                                               \
                    asynchronouslyLog(                                         \
                        message,                                               \
                        LEVEL,                                                 \
                        location,                                              \
                        std::chrono::system_clock::now());                     \
                                                                               \
                    util::debugBreak();                                        \
                                                                               \
                    throw std::runtime_error {std::move(message)};             \
                }                                                              \
                else                                                           \
                {                                                              \
                    asynchronouslyLog(                                         \
                        std::format(fmt, std::forward<Ts>(args)...),           \
                        LEVEL,                                                 \
                        location,                                              \
                        std::chrono::system_clock::now());                     \
                }                                                              \
            }                                                                  \
        }                                                                      \
    };                                                                         \
    template<class... J>                                                       \
    assert##LEVEL(bool, std::format_string<J...>, J&&...)                      \
        ->assert##LEVEL<J...>;

    MAKE_ASSERT(Trace, false) // NOLINT: We want implicit conversions
    MAKE_ASSERT(Debug, false) // NOLINT: We want implicit conversions
    MAKE_ASSERT(Log, false)   // NOLINT: We want implicit conversions
    MAKE_ASSERT(Warn, false)  // NOLINT: We want implicit conversions
    MAKE_ASSERT(Fatal, true)  // NOLINT: We want implicit conversions

#undef MAKE_ASSERT

    template<class... Ts>
    struct panic // NOLINT: we want to lie and pretend this is a function
    {
        panic( // NOLINT
            std::format_string<Ts...> fmt,
            Ts&&... args,
            const std::source_location& location =
                std::source_location::current())
        {
            using enum LoggingLevel;
            using namespace std::chrono_literals;
            std::string message = std::format(fmt, std::forward<Ts>(args)...);

            asynchronouslyLog(
                message,
                LoggingLevel::Panic,
                location,
                std::chrono::system_clock::now());

            util::debugBreak();

            throw std::runtime_error {message};
        }
    };
    template<class... J>
    panic(std::format_string<J...>, J&&...) -> panic<J...>;

} // namespace util
