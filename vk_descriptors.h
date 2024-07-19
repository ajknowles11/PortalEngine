#pragma once

#include <vector>
#include "vk_types.h"
#include <deque>
#include <span>

struct DescriptorLayoutBuilder {

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void addBinding(uint32_t binding, VkDescriptorType type);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages);
};

struct DescriptorAllocator {

	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
	void clearDescriptors(VkDevice device) const;
	void destroyPool(VkDevice device) const;

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout) const;
};

struct DescriptorAllocatorGrowable
{
	struct PoolSizeRatio
	{
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void clearPools(VkDevice device);
	void destroyPools(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
	VkDescriptorPool getPool(VkDevice device);
	static VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool = 4;
};

struct DescriptorWriter
{
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void writeImage(uint32_t binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void writeBuffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

	void clear();
	void updateSet(VkDevice device, VkDescriptorSet set);
};
