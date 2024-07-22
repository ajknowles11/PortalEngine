#include "camera.h"

#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"

void Camera::update(float delta)
{
	velocity.z = static_cast<float>(s_down - w_down);
	velocity.x = static_cast<float>(d_down - a_down);
	glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.0f)) * delta;
}

void Camera::processSDLEvent(SDL_Event& e)
{
	if (e.type == SDL_KEYDOWN)
	{
		if (e.key.keysym.sym == SDLK_w) { w_down = true; }
		if (e.key.keysym.sym == SDLK_s) { s_down = true; }
		if (e.key.keysym.sym == SDLK_a) { a_down = true; }
		if (e.key.keysym.sym == SDLK_d) { d_down = true; }
	}

	if (e.type == SDL_KEYUP)
	{
		if (e.key.keysym.sym == SDLK_w) { w_down = false; }
		if (e.key.keysym.sym == SDLK_s) { s_down = false; }
		if (e.key.keysym.sym == SDLK_a) { a_down = false; }
		if (e.key.keysym.sym == SDLK_d) { d_down = false; }
	}

	if (e.type == SDL_MOUSEMOTION)
	{
		yaw += static_cast<float>(e.motion.xrel) / 200.0f;
		pitch -= static_cast<float>(e.motion.yrel) / 200.0f;
	}
}

glm::mat4 Camera::getViewMatrix()
{
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 cameraRotation = getRotationMatrix();
	return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.0f, 0.0f, 0.0f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.0f, -1.0f, 0.0f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}
