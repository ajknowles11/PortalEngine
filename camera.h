#pragma once

#include "vk_types.h"
#include "SDL3/SDL_events.h"

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

	SDL_Window* window;

	glm::mat4 getViewMatrix();
	glm::mat4 getInvViewMatrix();
	glm::mat4 getRotationMatrix();

	void processSDLEvent(SDL_Event& e);

	void update(float delta);
private:
	Button wKey, sKey, aKey, dKey, qKey, eKey, shiftKey, rightClick;
};