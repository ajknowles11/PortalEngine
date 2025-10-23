#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform SceneData
{   
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 viewProj;
	mat4 cullViewProj;
} sceneData;

struct DirectionalLight
{
	vec3 direction;
	float intensity;
	vec3 color;
	float pad0;
};

layout (buffer_reference) readonly buffer DirectionalLights
{
	DirectionalLight lights[];
};

struct PointLight
{
	vec3 position;
	float pad0;
	vec3 color;
	
	float constant;
	float linear;
	float quadratic;

	float pad1, pad2;
};

layout (buffer_reference) readonly buffer PointLights
{
	PointLight lights[];
};

struct SpotLight
{
	vec3 position;
	float innerCutOff;
	vec3 direction;
	float outerCutOff;
	vec3 color;
	float intensity;
};

layout (buffer_reference) readonly buffer SpotLights
{
	SpotLight lights[];
};

layout (set = 0, binding = 1) uniform LightData
{
	uint directionalLightCount;
	uint pointLightCount;
	uint spotLightCount;
	float pad0;
	DirectionalLights directionalLights;
	PointLights pointLights;
	SpotLights spotLights;
} lightData;

layout (set = 0, binding = 2) uniform samplerCube environmentMap;
layout (set = 0, binding = 3) uniform samplerCube irradianceMap;
layout (set = 0, binding = 4) uniform samplerCube prefilterMap;
layout (set = 0, binding = 5) uniform sampler2D brdfLUT;

layout(set = 1, binding = 0) uniform PBRMaterialData
{  
	vec4 colorFactors;
	vec4 metalRoughFactors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metalRoughAOMap;
