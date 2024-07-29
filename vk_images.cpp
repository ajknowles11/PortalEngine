#include "vk_images.h"

#include "vk_initializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void vkUtil::transition_image(VkCommandBuffer const cmd, VkImage const image, VkImageLayout const currentLayout, VkImageLayout const newLayout)
{
	VkImageMemoryBarrier2 imageBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
		.oldLayout = currentLayout,
		.newLayout = newLayout
	};

	VkImageAspectFlags const aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange = vkInit::image_subresource_range(aspectMask);
	imageBarrier.image = image;

	VkDependencyInfo const depInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &imageBarrier
	};

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void vkUtil::copy_image_to_image(VkCommandBuffer const cmd, VkImage const source, VkImage const destination, VkExtent2D const srcSize, VkExtent2D const dstSize)
{
	VkImageBlit2 blitRegion
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
		.pNext = nullptr
	};

	blitRegion.srcOffsets[1].x = static_cast<int32_t>(srcSize.width);
	blitRegion.srcOffsets[1].y = static_cast<int32_t>(srcSize.height);
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = static_cast<int32_t>(dstSize.width);
	blitRegion.dstOffsets[1].y = static_cast<int32_t>(dstSize.height);
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 const blitInfo
	{
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.pNext = nullptr,
		.srcImage = source,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.dstImage = destination,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = 1,
		.pRegions = &blitRegion,
		.filter = VK_FILTER_LINEAR
	};

	vkCmdBlitImage2(cmd, &blitInfo);
}

void vkUtil::generate_mipmaps(VkCommandBuffer const cmd, VkImage const image, VkExtent2D imageSize)
{
	uint32_t const mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;
	for (uint32_t mip = 0; mip < mipLevels; mip++) {

		VkExtent2D halfSize = imageSize;
		halfSize.width /= 2;
		halfSize.height /= 2;

		VkImageAspectFlags constexpr aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageMemoryBarrier2 imageBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,

			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

			.image = image
		};

		imageBarrier.subresourceRange = vkInit::image_subresource_range(aspectMask);
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.baseMipLevel = mip;

		VkDependencyInfo const depInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageBarrier
		};

		vkCmdPipelineBarrier2(cmd, &depInfo);

		if (mip < mipLevels - 1) {
			VkImageBlit2 blitRegion
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
				.pNext = nullptr,
				.srcSubresource
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = mip,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.dstSubresource
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = mip + 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
			};

			blitRegion.srcOffsets[1].x = static_cast<int32_t>(imageSize.width);
			blitRegion.srcOffsets[1].y = static_cast<int32_t>(imageSize.height);
			blitRegion.srcOffsets[1].z = 1;

			blitRegion.dstOffsets[1].x = static_cast<int32_t>(halfSize.width);
			blitRegion.dstOffsets[1].y = static_cast<int32_t>(halfSize.height);
			blitRegion.dstOffsets[1].z = 1;

			VkBlitImageInfo2 const blitInfo
			{
				.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
				.pNext = nullptr,
				.srcImage = image,
				.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.dstImage = image,
				.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.regionCount = 1,
				.pRegions = &blitRegion,
				.filter = VK_FILTER_LINEAR
			};
			vkCmdBlitImage2(cmd, &blitInfo);

			imageSize = halfSize;
		}
	}

	// transition all mip levels into the final read_only layout
	transition_image(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
