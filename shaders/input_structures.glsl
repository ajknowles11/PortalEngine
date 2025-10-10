layout(set = 0, binding = 0) uniform SceneData
{   
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 viewProj;
	mat4 cullViewProj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 1, binding = 0) uniform PBRMaterialData
{  
	vec4 colorFactors;
	vec4 metalRoughFactors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metalRoughAOMap;
