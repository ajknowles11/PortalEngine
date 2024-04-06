#pragma once

namespace shaders
{
	uint32_t const gradientShaderRaw[] =
#include "shaders/gradient.comp.inl"
	;
	inline uint32_t const* gradientShader = gradientShaderRaw;
	size_t const gradientShaderSize = sizeof(gradientShaderRaw);

	uint32_t const skyShaderRaw[] =
#include "shaders/gradient.comp.inl"
		;
	inline uint32_t const* skyShader = skyShaderRaw;
	size_t const skyShaderSize = sizeof(skyShaderRaw);
}
