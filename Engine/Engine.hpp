#pragma once

#include "Module.hpp"
#include "Version.hpp" // If this is not found, run ```cmake .``` from root directory of this project.
#include "Log/Log.hpp"

#include <Math/Time/Time.hpp>
#include <LowLevel/Rocket.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <Application/App.hpp>

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

namespace SF::Engine
{
    /**
     * @brief Semantic version representation following semver.org conventions.
     */
    class Version
    {
    public:
        constexpr Version(uint8_t major = Engine_VERSION_MAJOR,
                          uint8_t minor = Engine_VERSION_MINOR,
                          uint8_t patch = Engine_VERSION_PATCH) noexcept
            : major(major), minor(minor), patch(patch)
        {
        }

        [[nodiscard]] constexpr bool operator==(const Version &other) const noexcept
        {
            return major == other.major && minor == other.minor && patch == other.patch;
        }

        [[nodiscard]] constexpr bool operator!=(const Version &other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const Version &other) const noexcept
        {
            if (major != other.major)
                return major < other.major;
            if (minor != other.minor)
                return minor < other.minor;
            return patch < other.patch;
        }

        [[nodiscard]] constexpr std::string_view ToString() const noexcept
        {
            // Simple compile-time string construction is tricky; return prebuilt string
            return Engine_VERSION;
        }

        uint8_t major;
        uint8_t minor;
        uint8_t patch;
    };

    // Provide a global constant version of the engine
    inline constexpr Version EngineVersion{};

    class Engine : NoCopy
    {
    public:
        /**
         * Gets the engines instance.
         * @return The current engine instance.
         */
        static Engine *Get() { return Instance; }

        /**
         * Carries out the setup for basic engine components and the engine. Call {@link Engine#Run} after creating a instance.
         * @param argv0 The first argument passed to main.
         * @param moduleFilter A filter for blacklisting/whitelisting modules.
         */
        explicit Engine(std::string argv0, ModuleFilter &&moduleFilter = {});
        ~Engine();

        /**
         * The update function for the updater.
         * @return {@code EXIT_SUCCESS} or {@code EXIT_FAILURE}
         */
        int32_t Run();

        /**
         * Gets the first argument passed to main.
         * @return The first argument passed to main.
         */
        const std::string &GetArgv0() const { return argv0; };

        /**
         * Gets the engine's version.
         * @return The engine's version.
         */
        const Version &GetVersion() const { return version; }

        /**
         * Gets the current application.
         * @return The renderer manager.
         */
        App *GetApp() const { return app.get(); }

        /**
         * Sets the current application to a new application.
         * @param app The new application.
         */
        void SetApp(std::unique_ptr<App> &&app) { this->app = std::move(app); }

        /**
         * Gets the fps limit.
         * @return The frame per second limit.
         */
        float GetFpsLimit() const { return fpsLimit; }

        /**
         * Sets the fps limit. -1 disables limits.
         * @param fpsLimit The new frame per second limit.
         */
        void SetFpsLimit(float fpsLimit) { this->fpsLimit = fpsLimit; }

        /**
         * Gets if the engine is running.
         * @return If the engine is running.
         */
        bool IsRunning() const { return running; }

        /**
         * Gets the delta (seconds) between updates.
         * @return The delta between updates.
         */
        const Time &GetDelta() const { return deltaUpdate.change; }

        /**
         * Gets the delta (seconds) between renders.
         * @return The delta between renders.
         */
        const Time &GetDeltaRender() const { return deltaRender.change; }

    private:
        UpdatesPerSecond<> ups, fps;

    public:
        /**
         * Gets the average UPS over a short interval.
         * @return The updates per second.
         */
        uint32_t GetUps() const { return ups.value_; }

        /**
         * Gets the average FPS over a short interval.
         * @return The frames per second.
         */
        uint32_t GetFps() const { return fps.value_; }

        /**
         * Requests the engine to stop the game-loop.
         */
        void RequestClose() { running = false; }

    private:
        void CreateModule(Module::RegistryMap::const_iterator it, const ModuleFilter &filter);
        void DestroyModule(TypeId id);
        void UpdateStage(Module::Stage stage);

        static Engine *Instance;

        std::string argv0;
        Version version;

        std::unique_ptr<App> app;

        std::map<TypeId, std::unique_ptr<Module>> modules;
        std::map<Module::Stage, std::vector<TypeId>> moduleStages;

        float fpsLimit;
        bool running;

        DeltaTime deltaUpdate, deltaRender;
        ElapsedTime elapsedUpdate, elapsedRender;
    };

}