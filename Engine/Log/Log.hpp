#pragma once

#include <cassert>
#include <memory>
#include <string_view>
#include <filesystem>

#include <Math/Time/Time.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace SF::Engine
{
    /**
     * @brief A logging class used in Engine, will write output to the standard stream and into a file.
     */
    class Log
    {
    public:
        // Color/style constants (compatible with spdlog's pattern formatters)
        struct Styles
        {
            constexpr static std::string_view Default = "";
            constexpr static std::string_view Bold = "";
            constexpr static std::string_view Dim = "";
            constexpr static std::string_view Underlined = "";
            constexpr static std::string_view Blink = "";
            constexpr static std::string_view Reverse = "";
            constexpr static std::string_view Hidden = "";
        };

        struct Colours
        {
            constexpr static std::string_view Default = "";
            constexpr static std::string_view Black = "";
            constexpr static std::string_view Red = "";
            constexpr static std::string_view Green = "";
            constexpr static std::string_view Yellow = "";
            constexpr static std::string_view Blue = "";
            constexpr static std::string_view Magenta = "";
            constexpr static std::string_view Cyan = "";
            constexpr static std::string_view LightGrey = "";
            constexpr static std::string_view DarkGrey = "";
            constexpr static std::string_view LightRed = "";
            constexpr static std::string_view LightGreen = "";
            constexpr static std::string_view LightYellow = "";
            constexpr static std::string_view LightBlue = "";
            constexpr static std::string_view LightMagenta = "";
            constexpr static std::string_view LightCyan = "";
            constexpr static std::string_view White = "";
        };

        constexpr static std::string_view TimestampFormat = "%H:%M:%S";

        /**
         * Initialize the logging system with default sinks (console and file)
         * @param filepath Path to the log file
         * @param name Logger name (default: "Engine")
         */
        static void Init(const std::filesystem::path &filepath = "logs/Engine.log",
                         const std::string &name = "Engine");

        /**
         * Shutdown the logging system
         */
        static void Shutdown();

        /**
         * Get the main logger instance
         */
        static std::shared_ptr<spdlog::logger> &GetLogger();

        /**
         * Outputs a message into the console.
         * @tparam Args The value types to write.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Out(Args &&...args)
        {
            if (auto logger = GetLogger())
            {
                logger->info(std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a debug message into the console.
         * @tparam Args The value types to write.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Debug(Args &&...args)
        {
            if (auto logger = GetLogger())
            {
                logger->debug(std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a info message into the console.
         * @tparam Args The value types to write.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Info(Args &&...args)
        {
            if (auto logger = GetLogger())
            {
                logger->info(std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a warning message into the console.
         * @tparam Args The value types to write.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Warning(Args &&...args)
        {
            if (auto logger = GetLogger())
            {
                logger->warn(std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a error message into the console.
         * @tparam Args The value types to write.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Error(Args &&...args)
        {
            if (auto logger = GetLogger())
            {
                logger->error(std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a critical message into the console.
         * @tparam Args The value types to write.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Critical(Args &&...args)
        {
            if (auto logger = GetLogger())
            {
                logger->critical(std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs an assert message into the console.
         * @tparam Args The value types to write.
         * @param expr The expression to assertion check.
         * @param args The values to write.
         */
        template <typename... Args>
        static void Assert(bool expr, Args &&...args)
        {
            if (!expr)
            {
                if (auto logger = GetLogger())
                {
                    logger->critical("Assertion failed: {}", std::forward<Args>(args)...);
                }
                assert(false);
            }
        }

        /**
         * Sets the log level for the logger
         * @param level The spdlog level to set
         */
        static void SetLevel(spdlog::level::level_enum level);

        /**
         * Sets the pattern for log messages
         * @param pattern The pattern string (spdlog format)
         */
        static void SetPattern(const std::string &pattern);

    private:
        static std::shared_ptr<spdlog::logger> s_Logger;
    };

    /**
     * @brief Base class for loggable objects that automatically add class name and instance info
     */
    template <typename T = std::nullptr_t>
    class Loggable
    {
    public:
        explicit Loggable(std::string &&className)
            : m_ClassName(std::move(className))
        {
        }

        template <typename = std::enable_if_t<!std::is_same_v<T, std::nullptr_t>>>
        Loggable()
            : Loggable(typeid(T).name())
        {
        }

        virtual ~Loggable() = default;

    protected:
        /**
         * Format a message with class name and instance information
         */
        template <typename... Args>
        std::string FormatMessage(Args &&...args) const
        {
            std::ostringstream oss;
            oss << "[" << m_ClassName << "]"
                << "(0x" << std::hex << std::uppercase
                << reinterpret_cast<uintptr_t>(this) << ") "
                << std::dec;

            // Use fold expression to append all arguments
            ((oss << std::forward<Args>(args)), ...);

            return oss.str();
        }

        template <typename... Args>
        void WriteOut(Args &&...args) const
        {
            if (auto logger = Log::GetLogger())
            {
                logger->info("{}", FormatMessage(std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteInfo(Args &&...args) const
        {
            if (auto logger = Log::GetLogger())
            {
                logger->info("INFO: {}", FormatMessage(std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteDebug(Args &&...args) const
        {
            if (auto logger = Log::GetLogger())
            {
                logger->debug("DEBUG: {}", FormatMessage(std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteWarning(Args &&...args) const
        {
            if (auto logger = Log::GetLogger())
            {
                logger->warn("WARN: {}", FormatMessage(std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteError(Args &&...args) const
        {
            if (auto logger = Log::GetLogger())
            {
                logger->error("ERROR: {}", FormatMessage(std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteCritical(Args &&...args) const
        {
            if (auto logger = Log::GetLogger())
            {
                logger->critical("CRITICAL: {}", FormatMessage(std::forward<Args>(args)...));
            }
        }

    private:
        std::string m_ClassName;
    };
} // namespace Engine

// Convenience macros for logging
#define ENGINE_LOG_TRACE(...) ::Engine::Log::GetLogger()->trace(__VA_ARGS__)
#define ENGINE_LOG_DEBUG(...) ::Engine::Log::GetLogger()->debug(__VA_ARGS__)
#define ENGINE_LOG_INFO(...) ::Engine::Log::GetLogger()->info(__VA_ARGS__)
#define ENGINE_LOG_WARN(...) ::Engine::Log::GetLogger()->warn(__VA_ARGS__)
#define ENGINE_LOG_ERROR(...) ::Engine::Log::GetLogger()->error(__VA_ARGS__)
#define ENGINE_LOG_CRITICAL(...) ::Engine::Log::GetLogger()->critical(__VA_ARGS__)

#define Engine_LOG_ASSERT(expr, ...)                                  \
    do                                                                \
    {                                                                 \
        if (!(expr))                                                  \
        {                                                             \
            ENGINE_LOG_CRITICAL("Assertion failed: {}", __VA_ARGS__); \
            assert(false);                                            \
        }                                                             \
    } while (0)