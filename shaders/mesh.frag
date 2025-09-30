#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

float CalcMipLevel(vec2 texCoord)
{
	vec2 dx = dFdx(texCoord);
	vec2 dy = dFdy(texCoord);
	float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
	
	return max(0.0, 0.5 * log2(delta_max_sqr));
}

void main() 
{
	vec4 texel = texture(colorTex, inUV);
	float scaledAlpha = texel.w * (1 + max(0.0, CalcMipLevel(inUV * textureSize(colorTex, 0))) * 0.25);
	if (scaledAlpha < 0.5) discard;
	vec3 texColor = texel.xyz / texel.w;

	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);

	vec3 color = inColor * texColor.xyz;
	vec3 ambient = color *  sceneData.ambientColor.xyz;

	outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient ,1.0f);
	//outFragColor = vec4(color, 1.0f);
}
