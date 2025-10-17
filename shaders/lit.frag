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

vec3 AddLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness)
{
	vec3 H = normalize(V + L);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);
	vec3 F = FresnelSchlick(max(dot(H, V), 0), F0);

	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);

	vec3 numerator = NDF * G * F;
	float denominator = 4.0 * max(dot(N, V), 0) * max(dot(N, L), 0) + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;

	kD *= 1.0 - metallic;

	float NdotL = max(dot(N, L), 0);
	return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() 
{
	// Mip-corrected alpha clip
	vec4 texel = texture(albedoMap, inUV);
	float scaledAlpha = texel.a * (1 + max(0.0, CalcMipLevel(inUV * textureSize(albedoMap, 0))) * 0.25);
	if (scaledAlpha < 0.5) discard;

	vec4 mrao = texture(metalRoughAOMap, inUV);

	vec3 albedo = texel.rgb / texel.a; // un-premultiply alpha
	albedo *= materialData.colorFactors.rgb;
	vec3 normal = GetNormalFromNormalMap();
	float metallic = mrao.b * materialData.metalRoughFactors.r;
	float roughness = mrao.g * materialData.metalRoughFactors.g;
	float ao = mrao.r;

	vec3 viewPos = vec3(sceneData.invView[3]);
	vec3 viewDir = normalize(viewPos - inFragPos);

	// integrate all light sources
	vec3 Lo = vec3(0);
	for (int i = 0; i < directionalLights.count; i++) 
	{
		DirectionalLight light = directionalLights.lights[i];

		vec3 L = normalize(-light.direction);

		vec3 radiance = light.color * light.intensity;

		Lo += AddLight(normal, viewDir, L, radiance, albedo, metallic, roughness);
	}
	for (int i = 0; i < pointLights.count; i++)
	{
		PointLight light = pointLights.lights[i];

		vec3 L = normalize(light.position - inFragPos);

		float dist = length(light.position - inFragPos);
		float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));
		vec3 radiance = light.color * attenuation;

		Lo += AddLight(normal, viewDir, L, radiance, albedo, metallic, roughness);
	}
	for (int i = 0; i < spotLights.count; i++)
	{
		SpotLight light = spotLights.lights[i];

		vec3 L = normalize(light.position - inFragPos);

		float theta = dot(L, normalize(-light.direction));
		if (theta > light.outerCutOff)
		{
			float epsilon = light.innerCutOff - light.outerCutOff;
			float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

			vec3 radiance = light.color * light.intensity;

			Lo += AddLight(normal, viewDir, L, radiance, albedo, metallic, roughness);
		}
	}

	vec3 ambient = sceneData.ambientColor.xyz * albedo * ao;
	vec3 color = ambient + Lo;
	outFragColor = vec4(color, texel.w * materialData.colorFactors.w);
}