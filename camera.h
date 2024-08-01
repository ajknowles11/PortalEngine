#pragma once

#include "vk_types.h"
#include "SDL_events.h"

struct Button
{
	bool pressed{};
	int downs{};
};

class Camera
{
public:
	glm::vec3 velocity;
	glm::vec3 position;
	float pitch{ 0.0f };
	float yaw{ 0.0f };

	glm::mat4 getViewMatrix();
	glm::mat4 getRotationMatrix();

	void processSDLEvent(SDL_Event& e);

	void update(float delta);
private:
	Button wKey, sKey, aKey, dKey, qKey, eKey, shiftKey, rightClick;
};