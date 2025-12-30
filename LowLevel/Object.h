#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <ID/GUID.hpp>

namespace SF::Engine
{
    class Object
    {
    public:
        // Constructor - generates new GUID
        Object() : guid(GUID::Generate()) {}

        // Copy constructor - generates NEW GUID (deep copy gets new ID)
        Object(const Object &other) : guid(GUID::Generate()) {}

        // Move constructor - transfers GUID
        Object(Object &&other) noexcept : guid(std::move(other.guid)) {}

        // Copy assignment - keeps original GUID (object identity preserved)
        Object &operator=(const Object &other)
        {
            // GUID remains the same - this is the same object
            return *this;
        }

        // Move assignment - transfers GUID
        Object &operator=(Object &&other) noexcept
        {
            if (this != &other)
            {
                guid = std::move(other.guid);
            }
            return *this;
        }

        // Virtual destructor for polymorphism
        virtual ~Object() = default;

        // Get the GUID
        const GUID &GetGUID() const { return guid; }

        // Get GUID as string
        std::string GetGUIDString() const { return guid.ToString(); }

        // Check if this is a null object
        bool IsNull() const { return guid.IsNull(); }

        // Compare objects by GUID
        bool operator==(const Object &other) const { return guid == other.guid; }
        bool operator!=(const Object &other) const { return guid != other.guid; }
        bool operator<(const Object &other) const { return guid < other.guid; }

        // For containers
        struct GUIDHash
        {
            size_t operator()(const Object &obj) const
            {
                return GUID::Hash()(obj.guid);
            }
        };

    protected:
        GUID guid;
    };
}

#endif // OBJECT_HPP