#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glslang/Public/ShaderLang.h>
#include <volk.h>

namespace SF::Engine::Shaders
{
    enum class ShaderLanguage
    {
        GLSL,
        HLSL
    };

    enum class ShaderStage
    {
        Vertex,
        Fragment,
        Compute,
        Geometry,
        TessellationControl,
        TessellationEvaluation
    };

    struct ComputeKernel
    {
        std::string name;
        std::string entryPoint;
        glm::uvec3 workgroupSize;
        bool hasWorkgroupSize = false;
    };

    struct ParsedShaderStage
    {
        ShaderStage stage;
        std::string source;
        std::string entryPoint = "main";

        // Compute-specific
        std::vector<ComputeKernel> kernels;
    };

    struct ParsedShader
    {
        std::string name;
        std::string filepath;
        ShaderLanguage language;

        std::vector<ParsedShaderStage> stages;
        std::vector<std::string> includes;

        // Properties
        std::map<std::string, std::string> stringProps;
        std::map<std::string, int> intProps;
        std::map<std::string, float> floatProps;
        std::map<std::string, bool> boolProps;
    };

    struct CompiledShader
    {
        std::string name;
        ShaderStage stage;
        ShaderLanguage language;
        std::vector<uint32_t> spirv;
        std::string entryPoint;

        // Compute-specific
        glm::uvec3 workgroupSize;
        bool hasWorkgroupSize = false;
    };

    class ShaderParser
    {
    public:
        ShaderParser();
        ~ShaderParser();

        // Parse shader file
        std::optional<ParsedShader> parse(const std::string &filepath);
        std::optional<ParsedShader> parseSource(const std::string &source, const std::string &name = "");

        // Compile parsed shader to SPIR-V
        std::optional<CompiledShader> compile(const ParsedShader &shader, ShaderStage stage);

        // Get last error
        const std::string &getLastError() const { return lastError_; }

    private:
        struct ParseContext
        {
            std::string source;
            size_t pos = 0;
            int line = 1;
            ParsedShader *shader = nullptr;
        };

        // Parsing methods
        bool parseDeclaration(ParseContext &ctx);
        bool parseStageBlock(ParseContext &ctx);
        bool parseComputeBlock(ParseContext &ctx);
        bool parseProperties(ParseContext &ctx);
        bool isShaderStageKeyword(const std::string &token);

        // Tokenization helpers
        void skipWhitespace(ParseContext &ctx);
        std::string readToken(ParseContext &ctx);
        std::string peekToken(ParseContext &ctx);
        std::string readQuotedString(ParseContext &ctx);
        std::string readUntil(ParseContext &ctx, char delim);

        // Stage processing
        std::string preprocessStage(const ParsedShader &shader, const ParsedShaderStage &stage);
        std::string stripComments(const std::string &source);

        // Compilation
        std::vector<uint32_t> compileGLSL(const std::string &source, ShaderStage stage,
                                          const std::string &entryPoint);
        std::vector<uint32_t> compileHLSL(const std::string &source, ShaderStage stage,
                                          const std::string &entryPoint);

        EShLanguage toGlslangStage(ShaderStage stage);
        std::string stageToDefine(ShaderStage stage);

        void setError(const std::string &error);

        std::string lastError_;
        bool glslangInitialized_ = false;
    };

    // Utility functions
    inline const char *stageToString(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "vertex";
        case ShaderStage::Fragment:
            return "fragment";
        case ShaderStage::Compute:
            return "compute";
        case ShaderStage::Geometry:
            return "geometry";
        case ShaderStage::TessellationControl:
            return "tess_control";
        case ShaderStage::TessellationEvaluation:
            return "tess_eval";
        default:
            return "unknown";
        }
    }

    inline std::optional<ShaderStage> stringToStage(const std::string &str)
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Handle shader stage keywords (with "Shader" suffix)
        if (lower == "vertexshader")
            return ShaderStage::Vertex;
        if (lower == "fragmentshader" || lower == "pixelshader")
            return ShaderStage::Fragment;
        if (lower == "computeshader")
            return ShaderStage::Compute;
        if (lower == "geometryshader")
            return ShaderStage::Geometry;
        if (lower == "tessellationcontrol" || lower == "tesellationcontrol")
            return ShaderStage::TessellationControl;
        if (lower == "tessellationeval" || lower == "tesellationeval")
            return ShaderStage::TessellationEvaluation;

        // Handle stage names without suffix
        if (lower == "vertex")
            return ShaderStage::Vertex;
        if (lower == "fragment" || lower == "pixel")
            return ShaderStage::Fragment;
        if (lower == "compute")
            return ShaderStage::Compute;
        if (lower == "geometry")
            return ShaderStage::Geometry;
        if (lower == "tesscontrol" || lower == "hull")
            return ShaderStage::TessellationControl;
        if (lower == "tesseval" || lower == "domain")
            return ShaderStage::TessellationEvaluation;

        return std::nullopt;
    }
}