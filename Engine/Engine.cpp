#include "Engine.hpp"

namespace SF::Engine
{
    Engine *Engine::Instance = nullptr;

    Engine::Engine(std::string argv0, ModuleFilter &&moduleFilter)
        : argv0(std::move(argv0)),
          version{Engine_VERSION_MAJOR, Engine_VERSION_MINOR, Engine_VERSION_PATCH},
          fpsLimit(-1.0f),
          running(true),
          elapsedUpdate(15.77ms),
          elapsedRender(-1s)
    {
        Instance = this;
        Log::Init(Time::GetDateTime("Logs/%Y%m%d%H%M%S.txt"));

        // Create modules from registry
        for (auto it = Module::Registry().begin(); it != Module::Registry().end(); ++it)
            CreateModule(it, moduleFilter);

        // Initialize all modules
        for (auto &[id, module] : modules)
        {
            if (module && !module->Initialize())
            {
                Log::Error("Failed to initialize module: {}", module->GetName());
            }
        }
    }

    Engine::~Engine()
    {
        app = nullptr;

        // Shutdown modules in reverse order
        for (auto it = modules.rbegin(); it != modules.rend(); ++it)
        {
            if (it->second)
                it->second->Shutdown();
        }

        // Destroy modules
        for (auto it = modules.rbegin(); it != modules.rend(); ++it)
            DestroyModule(it->first);

        Log::Shutdown();
        Instance = nullptr;
    }

    int32_t Engine::Run()
    {
        while (running)
        {
            if (app)
            {
                if (!app->started_)
                {
                    app->Start();
                    app->started_ = true;
                }

                app->Update();
            }

            elapsedRender.SetInterval(Time::Seconds(1.0f / fpsLimit));

            // Always-Update.
            UpdateStage(Module::Stage::Always);

            if (elapsedUpdate.GetElapsed() != 0)
            {
                // Resets the timer.
                ups.Update(Time::Now());

                // Pre-Update.
                UpdateStage(Module::Stage::Pre);
                // Update.
                UpdateStage(Module::Stage::Normal);
                // Post-Update.
                UpdateStage(Module::Stage::Post);

                // Updates the engines delta.
                deltaUpdate.Update();
            }

            // Renders when needed.
            if (elapsedRender.GetElapsed() != 0)
            {
                // Resets the timer.
                fps.Update(Time::Now());

                // Render
                UpdateStage(Module::Stage::Render);

                // Updates the render delta, and render time extension.
                deltaRender.Update();
            }
        }

        return EXIT_SUCCESS;
    }

    void Engine::CreateModule(Module::RegistryMap::const_iterator it, const ModuleFilter &filter)
    {
        // Check if module already exists
        if (modules.find(it->first) != modules.end())
            return;

        // Check if module is filtered out
        if (!filter.Check(it->first))
            return;

        // Recursively create dependencies first
        for (auto requireId : it->second.dependencies)
        {
            auto depIt = Module::Registry().find(requireId);
            if (depIt != Module::Registry().end())
                CreateModule(depIt, filter);
            else
                Log::Warning("Module dependency not found: TypeId {}", requireId);
        }

        // Create the module instance using the factory function
        auto module = it->second.createFunc();

        if (module)
        {
            Log::Info("Creating module: {}", it->second.name);
            modules[it->first] = std::move(module);
            moduleStages[it->second.stage].emplace_back(it->first);
        }
        else
        {
            Log::Error("Failed to create module: {}", it->second.name);
        }
    }

    void Engine::DestroyModule(TypeId id)
    {
        auto it = modules.find(id);
        if (it == modules.end() || !it->second)
            return;

        // Find and destroy all modules that depend on this module
        for (const auto &[registrarId, registrar] : Module::Registry())
        {
            // Check if any module depends on the module we're destroying
            auto depIt = std::find(registrar.dependencies.begin(),
                                   registrar.dependencies.end(), id);
            if (depIt != registrar.dependencies.end())
                DestroyModule(registrarId);
        }

        // Remove from stage list
        if (it->second)
        {
            auto stage = it->second->GetStage();
            auto &stageVec = moduleStages[stage];
            stageVec.erase(std::remove(stageVec.begin(), stageVec.end(), id), stageVec.end());
        }

        // Destroy the module
        modules.erase(it);
    }

    void Engine::UpdateStage(Module::Stage stage)
    {
        auto stageIt = moduleStages.find(stage);
        if (stageIt == moduleStages.end())
            return;

        for (auto &moduleId : stageIt->second)
        {
            auto modIt = modules.find(moduleId);
            if (modIt != modules.end() && modIt->second)
                modIt->second->Update();
        }
    }
}