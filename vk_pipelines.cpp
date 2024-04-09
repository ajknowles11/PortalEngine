#include "vk_pipelines.h"

#include "vk_initializers.h"

VkShaderModule vkUtil::load_shader_module(uint32_t const* shaderCode, size_t const codeSize, VkDevice const device)
{
	VkShaderModuleCreateInfo const createInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.codeSize = codeSize,
		.pCode = shaderCode
	};

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

	return shaderModule;
}

void PipelineBuilder::clear()
{
	inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	colorBlendAttachment = {};
	multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	pipelineLayout = {};
	depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	shaderStages.clear();
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice const device)
{
	VkPipelineViewportStateCreateInfo viewportState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineColorBlendStateCreateInfo colorBlending
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.dynamicStateCount = 2,
		.pDynamicStates = &state[0]
	};

	VkGraphicsPipelineCreateInfo const pipelineInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderInfo,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicInfo,
		.layout = pipelineLayout,
	};

	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
	{
		std::cerr << "Failed to create pipeline!\n";
		return VK_NULL_HANDLE;
	}
	else
	{
		return newPipeline;
	}
}

void PipelineBuilder::setShaders(VkShaderModule const vertexShader, VkShaderModule const fragmentShader)
{
	shaderStages.clear();
	shaderStages.push_back(vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
	shaderStages.push_back(vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology const topology)
{
	inputAssembly.topology = topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setPolygonMode(VkPolygonMode const mode)
{
	rasterizer.polygonMode = mode;
	rasterizer.lineWidth = 1.0f;
}

void PipelineBuilder::setCullMode(VkCullModeFlags const cullMode, VkFrontFace const frontFace)
{
	rasterizer.cullMode = cullMode;
	rasterizer.frontFace = frontFace;
}

void PipelineBuilder::setMultisamplingNone()
{
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disableBlending()
{
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::setColorAttachmentFormat(VkFormat const format)
{
	colorAttachmentFormat = format;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachmentFormats = &colorAttachmentFormat;
}

void PipelineBuilder::setDepthFormat(VkFormat const format)
{
	renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disableDepthTest()
{
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;
}
