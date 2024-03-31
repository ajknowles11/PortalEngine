#pragma once

#include "vk_types.h"

namespace vkInit {

	VkCommandPoolCreateInfo command_pool_create_info(uint32_t const queueFamilyIndex,
		VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(
		VkCommandPool const pool, uint32_t const count = 1);

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags const flags = 0);

	VkSemaphoreCreateInfo semaphore_create_info(VkFenceCreateFlags const flags = 0);

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags const flags = 0);

	VkImageSubresourceRange image_subresource_range(VkImageAspectFlags const aspectMask);

	VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 const stageMask, VkSemaphore const semaphore);

	VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer const cmd);

	VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo const* cmd, VkSemaphoreSubmitInfo const* signalSemaphoreInfo,
		VkSemaphoreSubmitInfo const* waitSemaphoreInfo);
}
