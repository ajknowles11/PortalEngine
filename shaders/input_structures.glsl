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

layout (set = 0, binding = 1) readonly buffer DirectionalLights
{
	uint count;
	DirectionalLight lights[];
} directionalLights;

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

layout (set = 0, binding = 2) readonly buffer PointLights
{
	uint count;
	PointLight lights[];
} pointLights;

struct SpotLight
{
	vec3 position;
	float innerCutOff;
	vec3 direction;
	float outerCutOff;
	vec3 color;
	float intensity;
};

layout (set = 0, binding = 3) readonly buffer SpotLights
{
	uint count;
	SpotLight lights[];
} spotLights;

layout (set = 0, binding = 4) uniform samplerCube environmentMap;
layout (set = 0, binding = 5) uniform samplerCube irradianceMap;

layout(set = 1, binding = 0) uniform PBRMaterialData
{  
	vec4 colorFactors;
	vec4 metalRoughFactors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metalRoughAOMap;
