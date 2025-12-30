#pragma once

#include <bitset>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <string_view>
#include <concepts>

#include <UtilityClasses/TypeInformation.hpp>
#include <UtilityClasses/NoCopy.hpp>

namespace SF::Engine
{
    // Forward declaration
    class Module;

    /**
     * @brief Concept to ensure a type is derived from Module
     */
    template <typename T>
    concept ModuleDerived = std::is_base_of_v<Module, T> && !std::is_same_v<Module, T>;

    /**
     * @brief Factory for creating and managing module instances
     */
    template <typename Base>
    class ModuleFactory
    {
    public:
        /**
         * @brief Creation information for a module
         */
        struct CreateInfo
        {
            std::function<std::unique_ptr<Base>()> createFunc;
            typename Base::Stage stage;
            std::vector<TypeId> dependencies;
            std::string_view name; // For debugging and logging
        };

        using RegistryMap = std::unordered_map<TypeId, CreateInfo>;

        virtual ~ModuleFactory() = default;

        /**
         * @brief Get the global module registry
         */
        static RegistryMap &Registry()
        {
            static RegistryMap impl;
            return impl;
        }

        /**
         * @brief Helper for specifying module dependencies
         */
        template <ModuleDerived... Args>
        class Requires
        {
        public:
            std::vector<TypeId> Get() const
            {
                std::vector<TypeId> dependencies;
                dependencies.reserve(sizeof...(Args));
                (dependencies.emplace_back(TypeInfo<Base>::template GetTypeId<Args>()), ...);
                return dependencies;
            }
        };

        /**
         * @brief Base registrar class for modules
         */
        template <typename T>
        class Registrar : public Base
        {
        public:
            virtual ~Registrar()
            {
                if (static_cast<T *>(this) == s_instance)
                    s_instance = nullptr;
            }

            static T *Get() noexcept { return s_instance; }
            static bool Exists() noexcept { return s_instance != nullptr; }

        protected:
            // We can enforce the concept here if we want, or rely on static_cast safety
            template <typename... Args> // relaxed for internal logic
            static bool Register(typename Base::Stage stage, Requires<Args...> dependencies = {})
            {
                // Optional: static_assert(std::is_base_of_v<Base, T>, "Class must derive from Module");

                constexpr std::string_view moduleName = typeid(T).name();

                ModuleFactory::Registry()[TypeInfo<Base>::template GetTypeId<T>()] = {
                    []() -> std::unique_ptr<Base>
                    {
                        s_instance = new T();
                        return std::unique_ptr<Base>(s_instance);
                    },
                    stage,
                    dependencies.Get(),
                    moduleName};

                return true;
            }

        private:
            inline static T *s_instance = nullptr;
        };

        template <typename T, typename... Args>
        static bool RegisterModule(
            typename Base::Stage stage,
            Requires<Args...> deps = {})
        {
            return Registrar<T>::Register(stage, deps);
        }

        template <typename T, typename... Args>
        struct AutoRegister
        {
            AutoRegister(
                typename Base::Stage stage,
                Requires<Args...> deps = {})
            {
                Registrar<T>::Register(stage, deps);
            }
        };
    }; // End of ModuleFactory class

    /**
     * @brief Base class for all engine modules
     */
    class Module : public ModuleFactory<Module>, NoCopy
    {
    public:
        /**
         * @brief Module update stages
         */
        enum class Stage : uint8_t
        {
            Never,  // Module is never updated (utility module)
            Always, // Module is always updated (critical systems)
            Pre,    // Early update (input, events)
            Normal, // Standard update (game logic)
            Post,   // Late update (physics, cleanup)
            Render  // Rendering stage
        };

        /**
         * @brief Stage and type identifier pair
         */
        using StageIndex = std::pair<Stage, TypeId>;

        virtual ~Module() = default;

        /**
         * @brief Update function called by the engine
         */
        virtual void Update() = 0;

        /**
         * @brief Optional initialization function
         * @return true if initialization succeeded, false otherwise
         */
        virtual bool Initialize() { return true; }

        /**
         * @brief Optional cleanup function
         */
        virtual void Shutdown() {}

        /**
         * @brief Get the module's update stage
         */
        virtual Stage GetStage() const = 0;

        /**
         * @brief Get the module's type ID
         */
        virtual TypeId GetTypeId() const = 0;

        /**
         * @brief Get the module's name (for debugging)
         */
        virtual std::string_view GetName() const = 0;
    };

    // Explicit template instantiation
    template class TypeInformation<Module>;

    /**
     * @brief Filter for selectively including/excluding modules
     */
    class ModuleFilter
    {
    public:
        static constexpr size_t MaxModules = 128; // Increased from 64

        ModuleFilter()
        {
            IncludeAll();
        }

        /**
         * @brief Check if a module type is included
         */
        template <ModuleDerived T>
        [[nodiscard]] bool Check() const noexcept
        {
            const auto id = TypeInfo<Module>::GetTypeId<T>();
            return id < MaxModules && m_include.test(id);
        }

        /**
         * @brief Check if a module type ID is included
         */
        [[nodiscard]] bool Check(TypeId typeId) const noexcept
        {
            return typeId < MaxModules && m_include.test(typeId);
        }

        /**
         * @brief Exclude a module type
         */
        template <ModuleDerived T>
        ModuleFilter &Exclude() noexcept
        {
            const auto id = TypeInfo<Module>::GetTypeId<T>();
            if (id < MaxModules)
                m_include.reset(id);
            return *this;
        }

        /**
         * @brief Include a module type
         */
        template <ModuleDerived T>
        ModuleFilter &Include() noexcept
        {
            const auto id = TypeInfo<Module>::GetTypeId<T>();
            if (id < MaxModules)
                m_include.set(id);
            return *this;
        }

        /**
         * @brief Exclude multiple module types
         */
        template <ModuleDerived... Args>
        ModuleFilter &Exclude() noexcept
        {
            (Exclude<Args>(), ...);
            return *this;
        }

        /**
         * @brief Include multiple module types
         */
        template <ModuleDerived... Args>
        ModuleFilter &Include() noexcept
        {
            (Include<Args>(), ...);
            return *this;
        }

        /**
         * @brief Exclude all modules
         */
        ModuleFilter &ExcludeAll() noexcept
        {
            m_include.reset();
            return *this;
        }

        /**
         * @brief Include all modules
         */
        ModuleFilter &IncludeAll() noexcept
        {
            m_include.set();
            return *this;
        }

        /**
         * @brief Get the number of included modules
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            return m_include.count();
        }

        /**
         * @brief Check if any modules are included
         */
        [[nodiscard]] bool Any() const noexcept
        {
            return m_include.any();
        }

        /**
         * @brief Check if all modules are included
         */
        [[nodiscard]] bool All() const noexcept
        {
            return m_include.all();
        }

    private:
        std::bitset<MaxModules> m_include;
    };

/**
 * @brief Helper macro for registering modules
 * Usage: REGISTER_MODULE(MyModule, Stage::Normal, Module::Requires<Dep1, Dep2>{})
 */
#define REGISTER_MODULE(ModuleClass, UpdateStage, ...) \
    inline static bool ModuleClass##_registered =      \
        ModuleClass::Register(UpdateStage, ##__VA_ARGS__)

} // namespace SF::Engine
