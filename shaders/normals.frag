#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPos;
layout (location = 4) in mat3 inTangentMat;

layout (location = 0) out vec4 outFragColor;

float CalcMipLevel(vec2 texCoord)
{
	vec2 dx = dFdx(texCoord);
	vec2 dy = dFdy(texCoord);
	float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
	
	return max(0.0, 0.5 * log2(delta_max_sqr));
}

vec3 GetNormalFromNormalMap()
{
	vec3 normal = texture(normalMap, inUV).rgb;
	normal = normal * 2.0 - 1.0;
	return (normalize(inTangentMat * normal) + 1.0) / 2.0;
}

void main() 
{
	// Mip-corrected alpha clip
	vec4 texel = texture(albedoMap, inUV);
	float scaledAlpha = texel.w * (1 + max(0.0, CalcMipLevel(inUV * textureSize(albedoMap, 0))) * 0.25);
	if (scaledAlpha < 0.5) discard;

	vec3 normal = GetNormalFromNormalMap();

	outFragColor = vec4(normal, 1.0);
}