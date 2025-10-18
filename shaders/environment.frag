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

// Based on http://www.oscars.org/science-technology/sci-tech-projects/aces
vec3 ACES(vec3 color)
{	
	mat3 m1 = mat3
	(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
	);
	mat3 m2 = mat3
	(
        1.60475, -0.10208, -0.00327,
        -0.53108,  1.10813, -0.07276,
        -0.07367, -0.00605,  1.07602
	);
	vec3 v = m1 * color;    
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return clamp(m2 * (a / b), 0.0, 1.0);
}

void main()
{
	vec3 envColor = texture(environmentMap, inLocalPos).rgb;

	envColor = ACES(envColor);

	outFragColor = vec4(envColor, 1.0);
}