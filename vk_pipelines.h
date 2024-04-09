#pragma once

#include <vector>

#include "vk_types.h"

namespace vkUtil
{
	VkShaderModule load_shader_module(uint32_t const *shaderCode, size_t codeSize, VkDevice device);
}

class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineRenderingCreateInfo renderInfo;
	VkFormat colorAttachmentFormat;

	PipelineBuilder() { clear(); };

	void clear();

	VkPipeline buildPipeline(VkDevice device);

	void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void setInputTopology(VkPrimitiveTopology topology);
	void setPolygonMode(VkPolygonMode mode);
	void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void setMultisamplingNone();
	void disableBlending();
	void setColorAttachmentFormat(VkFormat format);
	void setDepthFormat(VkFormat format);
	void disableDepthTest();
};
