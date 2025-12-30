#pragma once

#include <typeindex>
#include <unordered_map>
#include <type_traits>
#include <string_view>
#include <shared_mutex>

namespace SF::Engine
{
    using TypeId = std::size_t;

    /**
     * @brief Thread-safe type information system
     * @tparam T Base type for the type hierarchy
     */
    template <typename T>
    class TypeInformation
    {
    public:
        TypeInformation() = delete;

        /**
         * @brief Get the type ID for a derived type K
         * @tparam K The derived type (must be convertible to T*)
         * @return Unique type ID for K within the T hierarchy
         */
        template <typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static TypeId GetTypeId() noexcept
        {
            // Use double-checked locking pattern for thread safety
            const std::type_index typeIndex(typeid(K));

            // First check without lock (read)
            {
                std::shared_lock lock(s_mutex);
                if (auto it = s_typeMap.find(typeIndex); it != s_typeMap.end())
                    return it->second;
            }

            // Not found, acquire write lock
            std::unique_lock lock(s_mutex);

            // Double-check after acquiring write lock
            if (auto it = s_typeMap.find(typeIndex); it != s_typeMap.end())
                return it->second;

            // Create new ID
            const auto id = s_nextTypeId++;
            s_typeMap[typeIndex] = id;
            return id;
        }

        /**
         * @brief Get the type name for a derived type K
         * @tparam K The derived type
         * @return Type name as string_view
         */
        template <typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static constexpr std::string_view GetTypeName() noexcept
        {
            return typeid(K).name();
        }

        /**
         * @brief Get the number of registered types
         */
        [[nodiscard]] static size_t GetRegisteredTypeCount() noexcept
        {
            std::shared_lock lock(s_mutex);
            return s_typeMap.size();
        }

        /**
         * @brief Check if a type is registered
         */
        template <typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static bool IsRegistered() noexcept
        {
            std::shared_lock lock(s_mutex);
            const std::type_index typeIndex(typeid(K));
            return s_typeMap.find(typeIndex) != s_typeMap.end();
        }

        /**
         * @brief Clear all type registrations (use with caution!)
         */
        static void Clear() noexcept
        {
            std::unique_lock lock(s_mutex);
            s_typeMap.clear();
            s_nextTypeId = 0;
        }

    private:
        inline static TypeId s_nextTypeId = 0;
        inline static std::unordered_map<std::type_index, TypeId> s_typeMap = {};
        inline static std::shared_mutex s_mutex;
    };

    /**
     * @brief Alias for convenience
     */
    template <typename T>
    using TypeInfo = TypeInformation<T>;
}