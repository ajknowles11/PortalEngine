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

void DescriptorAllocatorGrowable::init(VkDevice const device, uint32_t const initialSets, std::span<PoolSizeRatio> const poolRatios)
{
	ratios.clear();

	for (auto const r : poolRatios)
	{
		ratios.push_back(r);
	}

	VkDescriptorPool const newPool = createPool(device, initialSets, poolRatios);

	setsPerPool = static_cast<uint32_t>(static_cast<float>(initialSets) * 1.5f);

	readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clearPools(VkDevice const device)
{
	for (auto const p : readyPools)
	{
		vkResetDescriptorPool(device, p, 0);
	}
	for (auto const p : fullPools)
	{
		vkResetDescriptorPool(device, p, 0);
		readyPools.push_back(p);
	}
	fullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(VkDevice const device)
{
	for (auto const p : readyPools)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	readyPools.clear();
	for (auto const p : fullPools)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	fullPools.clear();
}

VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice const device)
{
	VkDescriptorPool newPool;
	if (!readyPools.empty())
	{
		newPool = readyPools.back();
		readyPools.pop_back();
	}
	else
	{
		newPool = createPool(device, setsPerPool, ratios);
		setsPerPool = static_cast<uint32_t>(static_cast<float>(setsPerPool) * 1.5f);
		if (setsPerPool > 4092)
		{
			setsPerPool = 4092;
		}
	}

	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::createPool(VkDevice const device, uint32_t const setCount, std::span<PoolSizeRatio> const poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio const ratio : poolRatios)
	{
		poolSizes.push_back(VkDescriptorPoolSize
		{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount)
		});
	}

	VkDescriptorPoolCreateInfo const poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = setCount,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
	return newPool;
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice const device, VkDescriptorSetLayout layout, void* pNext)
{
	VkDescriptorPool poolToUse = getPool(device);

	VkDescriptorSetAllocateInfo allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = pNext,
		.descriptorPool = poolToUse,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout
	};

	VkDescriptorSet ds;

	if (VkResult const result = vkAllocateDescriptorSets(device, &allocInfo, &ds); result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
	{
		fullPools.push_back(poolToUse);
		poolToUse = getPool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	readyPools.push_back(poolToUse);
	return ds;
}

void DescriptorWriter::writeBuffer(uint32_t const binding, VkBuffer const buffer, size_t const size, size_t const offset, VkDescriptorType const type)
{
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo
		{
			.buffer = buffer,
			.offset = offset,
			.range = size
		});

	VkWriteDescriptorSet const write
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pBufferInfo = &info
	};

	writes.push_back(write);
}

void DescriptorWriter::writeImage(uint32_t const binding, VkImageView const image, VkSampler const sampler, VkImageLayout const layout, VkDescriptorType const type)
{
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo
	{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet const write
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pImageInfo = &info
	};

	writes.push_back(write);
}

void DescriptorWriter::clear()
{
	imageInfos.clear();
	writes.clear();
	bufferInfos.clear();
}

void DescriptorWriter::updateSet(VkDevice const device, VkDescriptorSet const set)
{
	for (VkWriteDescriptorSet& write : writes)
	{
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
