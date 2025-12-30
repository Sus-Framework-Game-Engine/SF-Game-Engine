// InstanceBuffer.cpp
#include "InstanceBuffer.hpp"
#include <algorithm>
#include <ranges>
#include <Graphics/Graphics.hpp>

namespace SF::Engine
{
	InstanceBuffer::InstanceBuffer(VkDeviceSize size)
		: Buffer(size,
				 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
	}

	void InstanceBuffer::Update(const CommandBuffer &commandBuffer,
								std::span<const std::byte> newData)
	{
		void *data;
		MapMemory(&data);
		std::ranges::copy(newData, static_cast<std::byte *>(data));
		UnmapMemory();
	}
}