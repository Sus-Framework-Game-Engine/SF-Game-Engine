#include "ShaderIncludes.hpp"
#include <iostream>
#include <regex>

namespace SF::Engine::Shaders
{
    ShaderIncludeResolver::ShaderIncludeResolver()
    {
        // Add default include directories
        includeDirs_.push_back(".");
        includeDirs_.push_back("./Shaders");
        includeDirs_.push_back("./Shaders/Include");
    }

    void ShaderIncludeResolver::addIncludeDirectory(const std::string &path)
    {
        std::string normalized = ShaderIncludeUtils::normalizePath(path);

        // Check if directory exists
        if (!std::filesystem::exists(normalized))
        {
            std::cerr << "Warning: Include directory does not exist: " << normalized << std::endl;
            return;
        }

        // Don't add duplicates
        if (std::find(includeDirs_.begin(), includeDirs_.end(), normalized) == includeDirs_.end())
        {
            includeDirs_.push_back(normalized);
        }
    }

    void ShaderIncludeResolver::clearIncludeDirectories()
    {
        includeDirs_.clear();
    }

    void ShaderIncludeResolver::clearCache()
    {
        includeCache_.clear();
        importedFiles_.clear();
    }

    std::string ShaderIncludeResolver::findIncludeFile(const std::string &filename, const std::string &basePath) const
    {
        // Try relative to base path first
        if (!basePath.empty())
        {
            std::string relativePath = ShaderIncludeUtils::combinePaths(basePath, filename);
            if (std::filesystem::exists(relativePath))
            {
                return std::filesystem::canonical(relativePath).string();
            }
        }

        // Try include directories
        for (const auto &dir : includeDirs_)
        {
            std::string fullPath = ShaderIncludeUtils::combinePaths(dir, filename);
            if (std::filesystem::exists(fullPath))
            {
                return std::filesystem::canonical(fullPath).string();
            }
        }

        // Try as absolute path
        if (std::filesystem::exists(filename))
        {
            return std::filesystem::canonical(filename).string();
        }

        return "";
    }

    std::optional<std::string> ShaderIncludeResolver::resolveIncludes(
        const std::string &source,
        const std::string &basePath,
        bool useImportSemantics)
    {
        std::set<std::string> processedFiles;

        // Clear imported files if not using import semantics
        if (!useImportSemantics)
        {
            importedFiles_.clear();
        }

        return resolveIncludesRecursive(source, basePath, processedFiles, 0);
    }

    std::optional<std::string> ShaderIncludeResolver::resolveIncludesRecursive(
        const std::string &source,
        const std::string &currentPath,
        std::set<std::string> &processedFiles,
        int depth)
    {
        if (depth > MAX_INCLUDE_DEPTH)
        {
            setError("Maximum include depth exceeded (" + std::to_string(MAX_INCLUDE_DEPTH) + ")");
            return std::nullopt;
        }

        std::stringstream result;
        std::istringstream sourceStream(source);
        std::string line;
        int lineNumber = 0;

        while (std::getline(sourceStream, line))
        {
            lineNumber++;
            bool isImport = false;
            std::string filename;

            // Check if this line is an include directive
            if (ShaderIncludeUtils::isIncludeDirective(line, isImport, filename))
            {
                // Find the include file
                std::string includeFilePath = findIncludeFile(filename, currentPath);

                if (includeFilePath.empty())
                {
                    setError("Failed to find include file: " + filename +
                             " (referenced at line " + std::to_string(lineNumber) + ")");
                    return std::nullopt;
                }

                // Normalize path for comparison
                includeFilePath = ShaderIncludeUtils::normalizePath(includeFilePath);

                // Check for circular dependencies
                if (processedFiles.find(includeFilePath) != processedFiles.end())
                {
                    if (trackDepth_)
                    {
                        result << "// [Circular include skipped: " << filename << "]\n";
                    }
                    continue;
                }

                // Check if already imported (for #import semantics)
                if (isImport && importedFiles_.find(includeFilePath) != importedFiles_.end())
                {
                    if (trackDepth_)
                    {
                        result << "// [Already imported: " << filename << "]\n";
                    }
                    continue;
                }

                // Load the include file
                std::optional<ShaderInclude> includeOpt = loadInclude(includeFilePath);
                if (!includeOpt)
                {
                    return std::nullopt; // Error already set
                }

                ShaderInclude &include = *includeOpt;

                // Mark as processed
                processedFiles.insert(includeFilePath);
                if (isImport)
                {
                    importedFiles_.insert(includeFilePath);
                }

                // Add line marker for debugging
                if (trackDepth_)
                {
                    result << "// [Begin include: " << filename << " (depth: " << depth << ")]\n";
                }
                else
                {
                    result << "// #include \"" << filename << "\"\n";
                }

                // Get directory of include file for nested includes
                std::string includeDir = ShaderIncludeUtils::getDirectory(includeFilePath);

                // Recursively process the included file
                auto processedInclude = resolveIncludesRecursive(
                    include.content,
                    includeDir,
                    processedFiles,
                    depth + 1);

                if (!processedInclude)
                {
                    return std::nullopt;
                }

                result << *processedInclude;

                if (trackDepth_)
                {
                    result << "// [End include: " << filename << "]\n";
                }

                // Remove from processed set (allow re-inclusion in different branches)
                processedFiles.erase(includeFilePath);
            }
            else
            {
                // Regular line, just copy it
                result << line << "\n";
            }
        }

        return result.str();
    }

    std::optional<ShaderInclude> ShaderIncludeResolver::loadInclude(const std::string &filepath)
    {
        // Check cache first
        auto it = includeCache_.find(filepath);
        if (it != includeCache_.end())
        {
            return it->second;
        }

        // Load from file
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            setError("Failed to open include file: " + filepath);
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        ShaderInclude include;
        include.path = filepath;
        include.content = buffer.str();
        include.processed = false;

        // Cache it
        includeCache_[filepath] = include;

        return include;
    }

    std::vector<std::string> ShaderIncludeResolver::getDependencies(const std::string &filepath)
    {
        std::vector<std::string> dependencies;
        std::set<std::string> visited;

        std::function<void(const std::string &)> collectDeps = [&](const std::string &file)
        {
            if (visited.find(file) != visited.end())
                return;

            visited.insert(file);

            auto includeOpt = loadInclude(file);
            if (!includeOpt)
                return;

            // Parse for include directives
            std::istringstream stream(includeOpt->content);
            std::string line;

            while (std::getline(stream, line))
            {
                bool isImport;
                std::string filename;

                if (ShaderIncludeUtils::isIncludeDirective(line, isImport, filename))
                {
                    std::string includeDir = ShaderIncludeUtils::getDirectory(file);
                    std::string includePath = findIncludeFile(filename, includeDir);

                    if (!includePath.empty())
                    {
                        dependencies.push_back(includePath);
                        collectDeps(includePath);
                    }
                }
            }
        };

        collectDeps(filepath);
        return dependencies;
    }

    std::string ShaderIncludeResolver::resolveIncludePath(const std::string &filename, const std::string &basePath) const
    {
        return findIncludeFile(filename, basePath);
    }

    bool ShaderIncludeResolver::hasCircularDependency(
        const std::string &filepath,
        const std::set<std::string> &processedFiles) const
    {
        return processedFiles.find(filepath) != processedFiles.end();
    }

    void ShaderIncludeResolver::setError(const std::string &error)
    {
        lastError_ = error;
        std::cerr << "ShaderIncludeResolver Error: " << error << std::endl;
    }

} // namespace SF::Engine::Shaders