#pragma once

namespace shaders
{
	uint32_t const gradientShaderRaw[] =
#include "shaders/gradient.comp.inl"
	;
	inline uint32_t const* gradientShader = gradientShaderRaw;
	size_t const gradientShaderSize = sizeof(gradientShaderRaw);

	uint32_t const skyShaderRaw[] =
#include "shaders/sky.comp.inl"
		;
	inline uint32_t const* skyShader = skyShaderRaw;
	size_t const skyShaderSize = sizeof(skyShaderRaw);

	uint32_t const coloredTriangleVertRaw[] =
#include "shaders/colored_triangle.vert.inl"
		;
	inline uint32_t const* coloredTriangleVert = coloredTriangleVertRaw;
	size_t const coloredTriangleVertSize = sizeof(coloredTriangleVertRaw);

	uint32_t const coloredTriangleFragRaw[] =
#include "shaders/colored_triangle.frag.inl"
		;
	inline uint32_t const* coloredTriangleFrag = coloredTriangleFragRaw;
	size_t const coloredTriangleFragSize = sizeof(coloredTriangleFragRaw);
}
