#include "vk_descriptors.h"

void DescriptorLayoutBuilder::addBinding(uint32_t const binding, VkDescriptorType const type)
{
	VkDescriptorSetLayoutBinding const newBind
	{
		.binding = binding,
		.descriptorType = type,
		.descriptorCount = 1
	};
	bindings.push_back(newBind);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice const device, VkShaderStageFlags const shaderStages)
{
	for (auto& b : bindings) 
	{
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::initPool(VkDevice const device, uint32_t const maxSets, std::span<PoolSizeRatio> const poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio const ratio : poolRatios) 
	{
		poolSizes.push_back(VkDescriptorPoolSize
			{
				.type = ratio.type,
				.descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets)
			});
	}

	VkDescriptorPoolCreateInfo const poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = maxSets,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
}

void DescriptorAllocator::clearDescriptors(VkDevice const device) const
{
	vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroyPool(VkDevice const device) const
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice const device, VkDescriptorSetLayout const layout) const
{
	VkDescriptorSetAllocateInfo const allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout
	};

	VkDescriptorSet ds;
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

	return ds;
}
