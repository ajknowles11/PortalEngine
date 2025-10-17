#pragma once

#include "vk_loader.h"

struct DirectionalLight
{
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	float const pad0 = 0;
};

struct PointLight
{
	glm::vec3 position;
	float const pad0 = 0;
	glm::vec3 color;
	
	float constant;
	float linear;
	float quadratic;

	float const pad1 = 0, pad2 = 0;
};

struct SpotLight
{
	glm::vec3 position;
	float innerCutOff;
	glm::vec3 direction;
	float outerCutOff;
	glm::vec3 color;
	float intensity;
};

class Scene 
{
public:
	std::shared_ptr<LoadedGLTF> staticGeometry;

	std::vector<DirectionalLight> directionalLights;
	std::vector<PointLight> pointLights;
	std::vector<SpotLight> spotLights;

	std::optional<AllocatedImage> environmentImage;
};