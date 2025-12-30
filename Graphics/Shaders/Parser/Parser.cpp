#include "Parser.hpp"
#include "ShaderIncludes.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang_standalone/StandAlone/DirStackFileIncluder.h>

namespace SF::Engine::Shaders
{
    ShaderParser::ShaderParser()
    {
        if (!glslangInitialized_)
        {
            glslang::InitializeProcess();
            glslangInitialized_ = true;
        }
    }

    ShaderParser::~ShaderParser()
    {
        if (glslangInitialized_)
        {
            glslang::FinalizeProcess();
            glslangInitialized_ = false;
        }
    }

    std::optional<ParsedShader> ShaderParser::parse(const std::string &filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            setError("Failed to open file: " + filepath);
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        auto result = parseSource(buffer.str(), filepath);
        if (result)
        {
            result->filepath = filepath;
        }
        return result;
    }

    std::optional<ParsedShader> ShaderParser::parseSource(const std::string &source, const std::string &name)
    {
        ParsedShader shader;
        shader.name = name.empty() ? "unnamed" : name;
        shader.language = ShaderLanguage::GLSL;

        ParseContext ctx;
        // First strip comments from entire source
        ctx.source = stripComments(source);
        ctx.shader = &shader;

        skipWhitespace(ctx);

        // Parse shader declaration: Shader "namespace/name"
        if (!parseDeclaration(ctx))
        {
            return std::nullopt;
        }

        // Expect opening brace for shader body
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after shader declaration");
            return std::nullopt;
        }
        ctx.pos++; // Skip '{'

        // Parse shader body
        while (ctx.pos < ctx.source.length())
        {
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.source.length())
                break;

            // Check for closing brace of shader body
            if (ctx.source[ctx.pos] == '}')
            {
                ctx.pos++;
                break;
            }

            std::string token = peekToken(ctx);

            if (token == "#import" || token == "#include")
            {
                readToken(ctx); // consume the token
                std::string includePath = readQuotedString(ctx);
                if (includePath.empty())
                {
                    // Try reading without quotes
                    includePath = readToken(ctx);
                }
                shader.includes.push_back(includePath);
            }
            else if (token == "#pragma")
            {
                readToken(ctx);
                std::string pragma = readUntil(ctx, '\n');
                if (pragma.find("hlsl") != std::string::npos)
                {
                    shader.language = ShaderLanguage::HLSL;
                }
            }
            else if (isShaderStageKeyword(token))
            {
                if (!parseStageBlock(ctx))
                {
                    return std::nullopt;
                }
            }
            else if (token == "inout" || token == "uniform" || token == "in" || token == "out")
            {
                // Skip variable declarations for now
                readUntil(ctx, ';');
            }
            else
            {
                // Skip unknown content
                ctx.pos++;
            }
        }

        return shader;
    }

    bool ShaderParser::parseDeclaration(ParseContext &ctx)
    {
        skipWhitespace(ctx);

        std::string token = readToken(ctx);
        if (token != "Shader" && token != "shader")
        {
            setError("Expected 'Shader' declaration, got: " + token);
            return false;
        }

        skipWhitespace(ctx);
        ctx.shader->name = readQuotedString(ctx);

        if (ctx.shader->name.empty())
        {
            setError("Shader name cannot be empty");
            return false;
        }

        return true;
    }

    bool ShaderParser::isShaderStageKeyword(const std::string &token)
    {
        return token == "VertexShader" || token == "FragmentShader" ||
               token == "ComputeShader" || token == "GeometryShader" ||
               token == "TessellationControl" || token == "TessellationEval" ||
               token == "TesellationControl" || token == "TesellationEval";
    }

    bool ShaderParser::parseStageBlock(ParseContext &ctx)
    {
        skipWhitespace(ctx);
        std::string stageStr = readToken(ctx);

        auto stageOpt = stringToStage(stageStr);
        if (!stageOpt)
        {
            setError("Unknown shader stage: " + stageStr);
            return false;
        }

        ParsedShaderStage stage;
        stage.stage = *stageOpt;

        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after stage declaration");
            return false;
        }
        ctx.pos++; // Skip '{'

        // Read until closing brace
        int braceDepth = 1;
        size_t start = ctx.pos;

        while (ctx.pos < ctx.source.length() && braceDepth > 0)
        {
            if (ctx.source[ctx.pos] == '{')
                braceDepth++;
            else if (ctx.source[ctx.pos] == '}')
                braceDepth--;

            if (braceDepth > 0)
                ctx.pos++;
        }

        stage.source = ctx.source.substr(start, ctx.pos - start);
        ctx.pos++; // Skip closing '}'

        ctx.shader->stages.push_back(stage);
        return true;
    }

    std::string ShaderParser::stripComments(const std::string &source)
    {
        return ShaderIncludeUtils::stripComments(source);
    }

    void ShaderParser::skipWhitespace(ParseContext &ctx)
    {
        while (ctx.pos < ctx.source.length() &&
               std::isspace(ctx.source[ctx.pos]))
        {
            if (ctx.source[ctx.pos] == '\n')
                ctx.line++;
            ctx.pos++;
        }
    }

    std::string ShaderParser::peekToken(ParseContext &ctx)
    {
        size_t savedPos = ctx.pos;
        std::string token = readToken(ctx);
        ctx.pos = savedPos;
        return token;
    }

    std::string ShaderParser::readToken(ParseContext &ctx)
    {
        skipWhitespace(ctx);

        std::string token;
        while (ctx.pos < ctx.source.length())
        {
            char c = ctx.source[ctx.pos];
            if (std::isalnum(c) || c == '_' || c == '#')
            {
                token += c;
                ctx.pos++;
            }
            else
            {
                break;
            }
        }
        return token;
    }

    std::string ShaderParser::readQuotedString(ParseContext &ctx)
    {
        skipWhitespace(ctx);

        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '"')
        {
            return "";
        }
        ctx.pos++; // Skip opening quote

        std::string str;
        while (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] != '"')
        {
            str += ctx.source[ctx.pos++];
        }

        if (ctx.pos < ctx.source.length())
        {
            ctx.pos++; // Skip closing quote
        }

        return str;
    }

    std::string ShaderParser::readUntil(ParseContext &ctx, char delim)
    {
        std::string result;
        while (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] != delim)
        {
            result += ctx.source[ctx.pos++];
        }
        if (ctx.pos < ctx.source.length())
            ctx.pos++; // Skip delimiter
        return result;
    }

    std::string ShaderParser::preprocessStage(const ParsedShader &shader, const ParsedShaderStage &stage)
    {
        std::string result;

        // Add version directive
        if (shader.language == ShaderLanguage::GLSL)
        {
            result += "#version 450 core\n";
        }

        // Add stage define
        result += stageToDefine(stage.stage) + "\n\n";

        // Resolve includes
        ShaderIncludeResolver resolver;

        // Add shader directory to include paths
        if (!shader.filepath.empty())
        {
            std::string shaderDir = ShaderIncludeUtils::getDirectory(shader.filepath);
            resolver.addIncludeDirectory(shaderDir);
        }

        // Process includes from shader declaration
        for (const auto &include : shader.includes)
        {
            std::string basePath = shader.filepath.empty() ? "" : ShaderIncludeUtils::getDirectory(shader.filepath);
            std::string includePath = resolver.findIncludeFile(include + ".si", basePath);

            if (includePath.empty())
            {
                std::cerr << "Warning: Include file not found: " << include << std::endl;
                result += "// Include not found: " + include + "\n";
            }
            else
            {
                std::ifstream includeFile(includePath);
                if (includeFile.is_open())
                {
                    std::stringstream buffer;
                    buffer << includeFile.rdbuf();
                    result += "// Begin include: " + include + "\n";
                    result += buffer.str();
                    result += "\n// End include: " + include + "\n\n";
                }
            }
        }

        // Add stage source
        result += stage.source;

        return result;
    }

    std::optional<CompiledShader> ShaderParser::compile(const ParsedShader &shader, ShaderStage stage)
    {
        // Find the stage in parsed shader
        const ParsedShaderStage *stagePtr = nullptr;
        for (const auto &s : shader.stages)
        {
            if (s.stage == stage)
            {
                stagePtr = &s;
                break;
            }
        }

        if (!stagePtr)
        {
            setError("Stage not found in shader: " + std::string(stageToString(stage)));
            return std::nullopt;
        }

        std::string processed = preprocessStage(shader, *stagePtr);

        std::vector<uint32_t> spirv;
        if (shader.language == ShaderLanguage::GLSL)
        {
            spirv = compileGLSL(processed, stage, stagePtr->entryPoint);
        }
        else
        {
            spirv = compileHLSL(processed, stage, stagePtr->entryPoint);
        }

        if (spirv.empty())
        {
            return std::nullopt;
        }

        CompiledShader compiled;
        compiled.name = shader.name;
        compiled.stage = stage;
        compiled.language = shader.language;
        compiled.spirv = std::move(spirv);
        compiled.entryPoint = stagePtr->entryPoint;

        return compiled;
    }

    std::vector<uint32_t> ShaderParser::compileGLSL(const std::string &source, ShaderStage stage,
                                                    const std::string &entryPoint)
    {
        EShLanguage glslStage = toGlslangStage(stage);

        glslang::TShader shader(glslStage);
        const char *sourcePtr = source.c_str();
        shader.setStrings(&sourcePtr, 1);

        shader.setEnvInput(glslang::EShSourceGlsl, glslStage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        const TBuiltInResource *resources = GetDefaultResources();
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

        DirStackFileIncluder includer;
        std::string preprocessed;

        if (!shader.preprocess(resources, 100, ENoProfile, false, false, messages, &preprocessed, includer))
        {
            setError("GLSL preprocessing failed:\n" + std::string(shader.getInfoLog()));
            return {};
        }

        const char *preprocessedPtr = preprocessed.c_str();
        shader.setStrings(&preprocessedPtr, 1);

        if (!shader.parse(resources, 100, false, messages))
        {
            setError("GLSL parsing failed:\n" + std::string(shader.getInfoLog()));
            return {};
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages))
        {
            setError("GLSL linking failed:\n" + std::string(program.getInfoLog()));
            return {};
        }

        std::vector<uint32_t> spirv;
        spv::SpvBuildLogger logger;
        glslang::SpvOptions options;
        glslang::GlslangToSpv(*program.getIntermediate(glslStage), spirv, &logger, &options);

        return spirv;
    }

    std::vector<uint32_t> ShaderParser::compileHLSL(const std::string &source, ShaderStage stage,
                                                    const std::string &entryPoint)
    {
        EShLanguage glslStage = toGlslangStage(stage);

        glslang::TShader shader(glslStage);
        const char *sourcePtr = source.c_str();
        shader.setStrings(&sourcePtr, 1);

        if (!entryPoint.empty())
        {
            shader.setEntryPoint(entryPoint.c_str());
        }
        else
        {
            shader.setEntryPoint("main");
        }

        shader.setEnvInput(glslang::EShSourceHlsl, glslStage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        const TBuiltInResource *resources = GetDefaultResources();
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);

        DirStackFileIncluder includer;
        std::string preprocessed;

        if (!shader.preprocess(resources, 100, ENoProfile, false, false, messages, &preprocessed, includer))
        {
            setError("HLSL preprocessing failed:\n" + std::string(shader.getInfoLog()));
            return {};
        }

        const char *preprocessedPtr = preprocessed.c_str();
        shader.setStrings(&preprocessedPtr, 1);

        if (!shader.parse(resources, 100, false, messages))
        {
            setError("HLSL parsing failed:\n" + std::string(shader.getInfoLog()));
            return {};
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages))
        {
            setError("HLSL linking failed:\n" + std::string(program.getInfoLog()));
            return {};
        }

        std::vector<uint32_t> spirv;
        spv::SpvBuildLogger logger;
        glslang::SpvOptions options;
        glslang::GlslangToSpv(*program.getIntermediate(glslStage), spirv, &logger, &options);

        return spirv;
    }

    EShLanguage ShaderParser::toGlslangStage(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return EShLangVertex;
        case ShaderStage::Fragment:
            return EShLangFragment;
        case ShaderStage::Compute:
            return EShLangCompute;
        case ShaderStage::Geometry:
            return EShLangGeometry;
        case ShaderStage::TessellationControl:
            return EShLangTessControl;
        case ShaderStage::TessellationEvaluation:
            return EShLangTessEvaluation;
        default:
            return EShLangVertex;
        }
    }

    std::string ShaderParser::stageToDefine(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "#define VERTEX_SHADER";
        case ShaderStage::Fragment:
            return "#define FRAGMENT_SHADER";
        case ShaderStage::Compute:
            return "#define COMPUTE_SHADER";
        case ShaderStage::Geometry:
            return "#define GEOMETRY_SHADER";
        case ShaderStage::TessellationControl:
            return "#define TESS_CONTROL_SHADER";
        case ShaderStage::TessellationEvaluation:
            return "#define TESS_EVAL_SHADER";
        default:
            return "";
        }
    }

    void ShaderParser::setError(const std::string &error)
    {
        lastError_ = error;
        std::cerr << "ShaderParser Error: " << error << std::endl;
    }

    std::optional<ShaderStage> stringToStage(const std::string &str)
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

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

        return std::nullopt;
    }
}