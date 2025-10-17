#version 450

layout(location = 0) in vec3 inLocalPos;

layout(set = 0, binding = 0) uniform  SceneData
{   
	mat4 view;
	mat4 invView;
	mat4 proj;
	mat4 viewProj;
	mat4 cullViewProj;
	vec4 ambientColor;
} sceneData;

layout (set = 1, binding = 0) uniform samplerCube environmentMap;

layout (location = 0) out vec4 outFragColor;

void main()
{
	vec3 envColor = texture(environmentMap, inLocalPos).rgb;

	envColor = envColor / (envColor + vec3(1.0));

	outFragColor = vec4(envColor, 1.0);
}