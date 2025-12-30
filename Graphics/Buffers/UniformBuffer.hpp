#pragma once

#include "Buffer.hpp"
#include <Graphics/Descriptors/BasicDescriptor.hpp>

namespace SF::Engine
{
	class UniformBuffer : public Descriptor, public Buffer
	{
	public:
		explicit UniformBuffer(VkDeviceSize size, const void *data = nullptr);

		void Update(const void *newData);

		WriteDescriptorSetInformation GetWriteDescriptor(uint32_t binding, VkDescriptorType descriptorType, const std::optional<OffsetSize> &offsetSize) const override;

		static VkDescriptorSetLayoutBinding GetDescriptorSetLayout(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stage, uint32_t count);
	};
}
