#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPos;
layout (location = 4) in mat3 inTangentMat;

layout (location = 0) out vec4 outFragColor;

#define PI 3.14159265358979

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
	return normalize(inTangentMat * normal);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) 
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0);
	float NdotL = max(dot(N, L), 0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

void main() 
{
	// Mip-corrected alpha clip
	vec4 texel = texture(albedoMap, inUV);
	float scaledAlpha = texel.w * (1 + max(0.0, CalcMipLevel(inUV * textureSize(albedoMap, 0))) * 0.25);
	if (scaledAlpha < 0.5) discard;

	vec4 mrao = texture(metalRoughAOMap, inUV);

	vec3 albedo = texel.xyz / texel.w; // un-premultiply alpha
	albedo *= materialData.colorFactors.xyz;
	vec3 normal = GetNormalFromNormalMap();
	float metallic = mrao.g * materialData.metalRoughFactors.x; //metal rough using gltf spec rn (g and b channels), convert to r and g later
	float roughness = mrao.b * materialData.metalRoughFactors.y;
	float ao = mrao.a;

	vec3 viewPos = vec3(sceneData.invView[3]);
	vec3 viewDir = normalize(viewPos - inFragPos);

	vec3 Lo = vec3(0);
	// integrate all light sources
	{
		vec3 L = normalize(-sceneData.sunlightDirection.xyz);
		vec3 H = normalize(viewDir + L);

		vec3 radiance = sceneData.sunlightColor.xyz * sceneData.sunlightDirection.w;

		vec3 F0 = vec3(0.04);
		F0 = mix(F0, albedo, metallic);
		vec3 F = FresnelSchlick(max(dot(H, viewDir), 0), F0);

		float NDF = DistributionGGX(normal, H, roughness);
		float G = GeometrySmith(normal, viewDir, L, roughness);

		vec3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(normal, viewDir), 0) * max(dot(normal, L), 0) + 0.0001;
		vec3 specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;

		kD *= 1.0 - metallic;

		float NdotL = max(dot(normal, L), 0);
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = sceneData.ambientColor.xyz * albedo * ao;
	vec3 color = ambient + Lo;
	outFragColor = vec4(color, texel.w * materialData.colorFactors.w);
}