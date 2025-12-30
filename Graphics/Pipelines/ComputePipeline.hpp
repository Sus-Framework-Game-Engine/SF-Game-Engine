#pragma once

#include <Graphics/Commands/CommandBuffer.hpp>
#include <Math/Vectors/Vector.hpp>
#include "Pipeline.hpp"
#include <filesystem>
#include <memory>

namespace SF::Engine
{
    // Forward declaration
    class Shader;

    /**
     * @brief Class that represents a compute pipeline.
     */
    class ComputePipeline : public Pipeline
    {
    public:
        /**
         * Creates a new compute pipeline.
         * @param shaderStage The shader file that will be loaded.
         * @param defines A list of defines added to the top of each shader.
         * @param pushDescriptors If no actual descriptor sets are allocated but instead pushed.
         */
        explicit ComputePipeline(
            std::filesystem::path shaderStage,
            std::vector<Shader::ShaderDefinition> definitions = {},
            bool pushDescriptors = false);

        ~ComputePipeline();

        void CmdRender(const CommandBuffer &commandBuffer, const Vector2Uint &extent) const;

        const std::filesystem::path &GetShaderStage() const { return shaderStage; }
        const std::vector<Shader::ShaderDefinition> &GetDefines() const { return defines; }
        bool IsPushDescriptors() const override { return pushDescriptors; }
        const Shader *GetShader() const override { return shader.get(); }
        const VkDescriptorSetLayout &GetDescriptorSetLayout() const override { return descriptorSetLayout; }
        const VkDescriptorPool &GetDescriptorPool() const override { return descriptorPool; }
        const VkPipeline &GetPipeline() const override { return pipeline; }
        const VkPipelineLayout &GetPipelineLayout() const override { return pipelineLayout; }
        const VkPipelineBindPoint &GetPipelineBindPoint() const override { return pipelineBindPoint; }

        // Helper methods for shader interaction
        template <typename T>
        void SetShaderVariable(const std::string &name, const T &value)
        {
            if (shader)
            {
                shader->setVariable(name, value);
            }
        }

        void ReloadShader(const std::vector<uint32_t> &newSpirv);

    private:
        void CreateShaderProgram();
        void CreateDescriptorLayout();
        void CreateDescriptorPool();
        void CreatePipelineLayout();
        void CreatePipelineCompute();

        void CleanupPipeline();
        void CleanupDescriptorLayout();
        void CleanupDescriptorPool();

        std::filesystem::path shaderStage;
        std::vector<Shader::ShaderDefinition> defines;
        bool pushDescriptors;

        std::shared_ptr<Shader> shader;

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    };
}