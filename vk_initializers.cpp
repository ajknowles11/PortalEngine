#include "vk_initializers.h"

VkCommandPoolCreateInfo vkInit::command_pool_create_info(uint32_t const queueFamilyIndex,
	VkCommandPoolCreateFlags const flags)
{
	VkCommandPoolCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
		.queueFamilyIndex = queueFamilyIndex
	};
	return info;
}


VkCommandBufferAllocateInfo vkInit::command_buffer_allocate_info(
	VkCommandPool const pool, uint32_t const count)
{
	VkCommandBufferAllocateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = count
	};
	return info;
}

VkFenceCreateInfo vkInit::fence_create_info(VkFenceCreateFlags const flags)
{
	VkFenceCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags
	};
	return info;
}

VkSemaphoreCreateInfo vkInit::semaphore_create_info(VkSemaphoreCreateFlags const flags)
{
	VkSemaphoreCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags
	};
	return info;
}

VkCommandBufferBeginInfo vkInit::command_buffer_begin_info(VkCommandBufferUsageFlags const flags)
{
	VkCommandBufferBeginInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = flags,
		.pInheritanceInfo = nullptr
	};
	return info;
}

VkImageSubresourceRange vkInit::image_subresource_range(VkImageAspectFlags const aspectMask)
{
	VkImageSubresourceRange const subImage
	{
		.aspectMask = aspectMask,
		.baseMipLevel = 0,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0,
		.layerCount = VK_REMAINING_ARRAY_LAYERS
	};
	return subImage;
}

VkSemaphoreSubmitInfo vkInit::semaphore_submit_info(VkPipelineStageFlags2 const stageMask, VkSemaphore const semaphore)
{
	VkSemaphoreSubmitInfo const submitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = semaphore,
		.value = 1,
		.stageMask = stageMask,
		.deviceIndex = 0
	};
	return submitInfo;
}

VkCommandBufferSubmitInfo vkInit::command_buffer_submit_info(VkCommandBuffer const cmd)
{
	VkCommandBufferSubmitInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext = nullptr,
		.commandBuffer = cmd,
		.deviceMask = 0
	};
	return info;
}

VkSubmitInfo2 vkInit::submit_info(VkCommandBufferSubmitInfo const* cmd, VkSemaphoreSubmitInfo const* signalSemaphoreInfo,
	VkSemaphoreSubmitInfo const* waitSemaphoreInfo)
{
	VkSubmitInfo2 const info
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u,
		.pWaitSemaphoreInfos = waitSemaphoreInfo,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = cmd,
		.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u,
		.pSignalSemaphoreInfos = signalSemaphoreInfo,
	};
	return info;
}

VkImageCreateInfo vkInit::image_create_info(VkFormat const format, VkImageUsageFlags const usageFlags, VkExtent3D const extent)
{
	VkImageCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = extent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usageFlags
	};
	return info;
}

VkImageViewCreateInfo vkInit::image_view_create_info(VkFormat const format, VkImage const image, VkImageAspectFlags const aspectFlags)
{
	VkImageViewCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = 
		{
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
	return info;
}

VkRenderingAttachmentInfo vkInit::attachment_info(VkImageView const view, VkClearValue const* clear, VkImageLayout const layout)
{
	VkRenderingAttachmentInfo colorAttachment
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = view,
		.imageLayout = layout,
		.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};

	if (clear) {
		colorAttachment.clearValue = *clear;
	
	}

	return colorAttachment;
}

VkRenderingAttachmentInfo vkInit::depth_attachment_info(VkImageView const view, VkImageLayout const layout)
{
	VkRenderingAttachmentInfo const depthAttachment
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = view,
		.imageLayout = layout,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue =
		{
			.depthStencil =
			{
				.depth = 0.0f
			}
		}
	};

	return depthAttachment;
}

VkRenderingInfo vkInit::rendering_info(VkExtent2D const renderExtent, VkRenderingAttachmentInfo const* colorAttachment,
	VkRenderingAttachmentInfo const* depthAttachment)
{
	VkRenderingInfo const renderInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = nullptr,
		.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, renderExtent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = colorAttachment,
		.pDepthAttachment = depthAttachment,
		.pStencilAttachment = nullptr
	};
	return renderInfo;
}

VkPipelineLayoutCreateInfo vkInit::pipeline_layout_create_info()
{
	VkPipelineLayoutCreateInfo constexpr info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,

		.flags = 0,
		.setLayoutCount = 0,
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};
	return info;
}

VkPipelineShaderStageCreateInfo vkInit::pipeline_shader_stage_create_info(VkShaderStageFlagBits const stage,
	VkShaderModule const shaderModule,
	const char* entry)
{
	VkPipelineShaderStageCreateInfo const info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,

		.stage = stage,
		.module = shaderModule,
		.pName = entry
	};
	return info;
}
