#pragma once

#include <Files/File.hpp>
#include <LowLevel/Rocket.hpp>
#include <string>
#include <locale>
#include <codecvt>

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

#if _PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__APPLE__)
// macOS
#include <mach-o/dyld.h>
#include <vector>
#elif defined(__linux__)
// Linux
#include <unistd.h>
#include <vector>
#endif

namespace SF::Engine
{
    class Version
    {
    public:
        Version(uint8_t major, uint8_t minor, uint8_t patch) : major(major),
                                                               minor(minor),
                                                               patch(patch)
        {
        }

        uint8_t major, minor, patch;
    };

    /**
     * @brief Base class representing an application with lifecycle management.
     *
     * Applications can be started, updated, and switched between. Each app
     * has a name and version for identification and driver support.
     */

    class App : public virtual rocket::trackable
    {
        friend class Engine;

    public:
        explicit App(std::string name, const Version &version = {1, 0, 0})
            : name_(std::move(name)), version_(version)
        {
        }

        virtual ~App() = default;

        // Prevent copying, allow moving
        App(const App &) = delete;
        App &operator=(const App &) = delete;
        App(App &&) noexcept = default;
        App &operator=(App &&) noexcept = default;

        /**
         * @brief Called when switching to this app from another.
         *
         * Use this method to initialize resources and prepare the application
         * for active use. Will only be called once per activation.
         */
        virtual void Start() = 0;

        /**
         * @brief Called each frame before the module update pass.
         *
         * Implement application-specific logic and state updates here.
         * This method is called continuously while the app is active.
         */
        virtual void Update() = 0;

        /**
         * @brief Gets the application's name.
         * @return The application's name.
         */
        [[nodiscard]] const std::string &GetName() const noexcept { return name_; }

        /**
         * @brief Sets the application's name for driver support.
         * @param name The new application name.
         */
        void SetName(std::string_view name) { name_ = name; }

        /**
         * @brief Gets the application's version.
         * @return The application's version.
         */
        [[nodiscard]] const Version &GetVersion() const noexcept { return version_; }

        /**
         * @brief Sets the application's version for driver support.
         * @param version The new application version.
         */
        void SetVersion(const Version &version) noexcept { version_ = version; }

        /**
         * @brief Checks if the application has been started.
         * @return True if Start() has been called, false otherwise.
         */
        [[nodiscard]] bool IsStarted() const noexcept { return started_; }

        std::filesystem::path GetExecutablePath();

    private:
        std::vector<File> modules;

    public:
        std::vector<File> GetAllModules() const
        {
            std::vector<File> result;

            const auto exeDir =
                GetExecutablePath().parent_path();

            const char *pattern = "*.module";
            for (const auto &file : File::GetFiles(exeDir, pattern, false))
            {
                result.emplace_back(file);
            }

            return result;
        }

    private:
        bool started_ = false;
        std::string name_;
        Version version_;
        std::string path;
    };
}
/*
App Dir example:
image: https://i.imgur.com/1n0bX4l.png

*/