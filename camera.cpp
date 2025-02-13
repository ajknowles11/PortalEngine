#include "camera.h"

#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"

void Camera::update(float delta)
{
	if (rightClick.pressed)
	{
		velocity.z = static_cast<float>(sKey.pressed - wKey.pressed); //because z points toward camera
		velocity.x = static_cast<float>(dKey.pressed - aKey.pressed);
		velocity.y = static_cast<float>(eKey.pressed - qKey.pressed);
		glm::mat4 const cameraRotation = getRotationMatrix();
		velocity *= shiftKey.pressed ? 10.0f : 2.0f;
		position += glm::vec3(cameraRotation * glm::vec4(velocity, 0.0f)) * delta;
	}
	else
	{
		velocity = glm::vec3();
	}

	wKey.downs = 0;
	sKey.downs = 0;
	aKey.downs = 0;
	dKey.downs = 0;
	qKey.downs = 0;
	eKey.downs = 0;
	shiftKey.downs = 0;
}

void Camera::processSDLEvent(SDL_Event& e)
{
	if (e.type == SDL_KEYDOWN)
	{
		if (e.key.keysym.sym == SDLK_w) { wKey.pressed = true; wKey.downs += 1; }
		if (e.key.keysym.sym == SDLK_s) { sKey.pressed = true; sKey.downs += 1; }
		if (e.key.keysym.sym == SDLK_a) { aKey.pressed = true; aKey.downs += 1; }
		if (e.key.keysym.sym == SDLK_d) { dKey.pressed = true; dKey.downs += 1;}

		if (e.key.keysym.sym == SDLK_q) { qKey.pressed = true; qKey.downs += 1;}
		if (e.key.keysym.sym == SDLK_e) { eKey.pressed = true; eKey.downs += 1;}

		if (e.key.keysym.sym == SDLK_LSHIFT) { shiftKey.pressed = true; shiftKey.downs += 1;}
	}

	if (e.type == SDL_KEYUP)
	{
		if (e.key.keysym.sym == SDLK_w) { wKey.pressed = false; }
		if (e.key.keysym.sym == SDLK_s) { sKey.pressed = false; }
		if (e.key.keysym.sym == SDLK_a) { aKey.pressed = false; }
		if (e.key.keysym.sym == SDLK_d) { dKey.pressed = false; }

		if (e.key.keysym.sym == SDLK_q) { qKey.pressed = false; }
		if (e.key.keysym.sym == SDLK_e) { eKey.pressed = false; }

		if (e.key.keysym.sym == SDLK_LSHIFT) { shiftKey.pressed = false; }
	}

	if (e.type == SDL_MOUSEBUTTONDOWN)
	{
		if (e.button.button == SDL_BUTTON_RIGHT)
		{
			rightClick.pressed = true;
			rightClick.downs += 1;
			SDL_SetRelativeMouseMode(SDL_TRUE);
			SDL_ShowCursor(SDL_DISABLE);
		}
	}

	if (e.type == SDL_MOUSEBUTTONUP)
	{
		if (e.button.button == SDL_BUTTON_RIGHT)
		{
			rightClick.pressed = false;
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
	}

	if (e.type == SDL_MOUSEMOTION)
	{
		if (rightClick.pressed)
		{
			yaw += static_cast<float>(e.motion.xrel) / 200.0f;
			pitch -= static_cast<float>(e.motion.yrel) / 200.0f;
		}
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
