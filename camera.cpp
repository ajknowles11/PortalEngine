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
	if (e.type == SDL_EVENT_KEY_DOWN)
	{
		if (e.key.key == SDLK_W) { wKey.pressed = true; wKey.downs += 1; }
		if (e.key.key == SDLK_S) { sKey.pressed = true; sKey.downs += 1; }
		if (e.key.key == SDLK_A) { aKey.pressed = true; aKey.downs += 1; }
		if (e.key.key == SDLK_D) { dKey.pressed = true; dKey.downs += 1;}

		if (e.key.key == SDLK_Q) { qKey.pressed = true; qKey.downs += 1;}
		if (e.key.key == SDLK_E) { eKey.pressed = true; eKey.downs += 1;}

		if (e.key.key == SDLK_LSHIFT) { shiftKey.pressed = true; shiftKey.downs += 1;}
	}

	if (e.type == SDL_EVENT_KEY_UP)
	{
		if (e.key.key == SDLK_W) { wKey.pressed = false; }
		if (e.key.key == SDLK_S) { sKey.pressed = false; }
		if (e.key.key == SDLK_A) { aKey.pressed = false; }
		if (e.key.key == SDLK_D) { dKey.pressed = false; }

		if (e.key.key == SDLK_Q) { qKey.pressed = false; }
		if (e.key.key == SDLK_E) { eKey.pressed = false; }

		if (e.key.key == SDLK_LSHIFT) { shiftKey.pressed = false; }
	}

	if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
	{
		if (e.button.button == SDL_BUTTON_RIGHT)
		{
			rightClick.pressed = true;
			rightClick.downs += 1;
			SDL_SetWindowRelativeMouseMode(window, true);
			SDL_HideCursor();
		}
	}

	if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
	{
		if (e.button.button == SDL_BUTTON_RIGHT)
		{
			rightClick.pressed = false;
			SDL_SetWindowRelativeMouseMode(window, false);
			SDL_ShowCursor();
		}
	}

	if (e.type == SDL_EVENT_MOUSE_MOTION)
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

glm::mat4 Camera::getInvViewMatrix()
{
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 cameraRotation = getRotationMatrix();
	return cameraTranslation * cameraRotation;
}

glm::mat4 Camera::getRotationMatrix()
{
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.0f, 0.0f, 0.0f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.0f, -1.0f, 0.0f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}
