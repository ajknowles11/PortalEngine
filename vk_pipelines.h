#pragma once

#include <vector>

#include "vk_types.h"

namespace vkUtil
{
	VkShaderModule load_shader_module(uint32_t const *shaderCode, size_t codeSize, VkDevice device);
}
