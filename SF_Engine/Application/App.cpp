#include <filesystem>
#include <vector>
#include <string>
#include <App.hpp>

namespace SF::Engine
{
    inline std::filesystem::path App::GetExecutablePath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(MAX_PATH);

        DWORD size = 0;
        while (true)
        {
            size = GetModuleFileNameW(nullptr, buffer.data(),
                                      static_cast<DWORD>(buffer.size()));

            if (size == 0)
                return {};

            if (size < buffer.size())
                break;

            buffer.resize(buffer.size() * 2);
        }

        return std::filesystem::path(buffer.data());

#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);

        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            return {};

        return std::filesystem::weakly_canonical(buffer.data());

#elif defined(__linux__)
        std::vector<char> buffer(1024);
        ssize_t size = 0;

        while (true)
        {
            size = readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (size < 0)
                return {};

            if (size < static_cast<ssize_t>(buffer.size()))
                break;

            buffer.resize(buffer.size() * 2);
        }

        return std::filesystem::path(
            std::string(buffer.data(), size));

#else
        return {};
#endif
    }
}
