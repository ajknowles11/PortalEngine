#include "vk_pipelines.h"

#include "vk_initializers.h"

VkShaderModule vkUtil::load_shader_module(uint32_t const* shaderCode, size_t codeSize, VkDevice device)
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
