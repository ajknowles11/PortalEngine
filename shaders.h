#pragma once

namespace shaders
{
	uint32_t const gradientShaderRaw[] =
#include "shaders/gradient.comp.inl"
	;
	inline uint32_t const* gradientShader = gradientShaderRaw;
	size_t const gradientShaderSize = sizeof(gradientShaderRaw);
}
