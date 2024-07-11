#pragma once
#include <format>
#include <iostream>
#include <math/Algebra.hpp>

namespace Log
{

enum class Severity
{
    NONE, // Applies no formatting or colours
    INFO, // Use for debugging and general info
    WARN, // Use when something is out of place, but still recoverable
    FATAL // Use when crashing is what needs to happen
};

namespace detail
{

    inline std::ostream* output = &std::cout;

    // Text Colours
    static constexpr auto green = "\033[32m";
    static constexpr auto yellow = "\033[33m";
    static constexpr auto red = "\033[31m";
    static constexpr auto reset = "\033[0m";

    // Type message
    inline const char* format_message_type(Severity type)
    {
        switch (type)
        {
        case Severity::INFO:
            return "\033[32m"
                   "INFO: "
                   "\033[0m";
        case Severity::WARN:
            return "\033[33m"
                   "WARN:"
                   "\033[0m";
        case Severity::FATAL:
            return "\033[31m"
                   "ERROR:"
                   "\033[0m";
        default:
            return "";
        }
    }

    inline std::string reduce_path(const std::string& in_path)
    {
        return in_path.substr(in_path.find("KSEngine"));
    }

} // namespace detail

template <typename FormatString, typename... Args>
inline void Message(Severity type, FormatString&& fmt, Args&&... args)
{
    *detail::output << detail::format_message_type(type)
                    << std::vformat(std::forward<FormatString>(fmt), std::make_format_args(args...));
}

inline void Break() { *detail::output << "\n"; }

} // namespace Log

#define LOG(severity, message, ...)                                \
    {                                                              \
        Log::Message(severity,                                     \
            "{} at Line {}: ", Log::detail::reduce_path(__FILE__), \
            __LINE__);                                             \
        Log::Message(Log::Severity::NONE, message, __VA_ARGS__);   \
        Log::Break();                                              \
    }

namespace std
{

// Vec2
template <>
struct formatter<glm::vec2> : std::formatter<std::string>
{
    auto format(glm::vec2 p, format_context& ctx) const
    {
        return formatter<string>::format(
            std::format("[{}, {}]", p.x, p.y), ctx);
    }
};

// Vec3
template <>
struct formatter<glm::vec3> : std::formatter<std::string>
{
    auto format(glm::vec3 p, format_context& ctx) const
    {
        return formatter<string>::format(
            std::format("[{}, {}, {}]", p.x, p.y, p.z), ctx);
    }
};

// Vec4
template <>
struct formatter<glm::vec4> : std::formatter<std::string>
{
    auto format(glm::vec4 p, format_context& ctx) const
    {
        return formatter<string>::format(
            std::format("[{}, {}, {}, {}]", p.x, p.y, p.z, p.w), ctx);
    }
};

// Mat4
template <>
struct formatter<glm::mat4> : std::formatter<std::string>
{
    auto format(glm::mat4 p, format_context& ctx) const
    {
        return formatter<std::string>::format(
            std::format("\n{}\n{}\n{}\n{}\n", p[0], p[1], p[2], p[3]), ctx);
    }
};

// Quat
template <>
struct formatter<glm::quat> : std::formatter<std::string>
{
    auto format(glm::vec4 p, format_context& ctx) const
    {
        return formatter<string>::format(
            std::format("[{}, {}, {}, {}]", p.x, p.y, p.z, p.w), ctx);
    }
};
}