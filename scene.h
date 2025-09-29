#pragma once

#include "vk_loader.h"

class Scene 
{
	Scene(std::string_view path);

	std::shared_ptr<LoadedGLTF> staticGeometry;
};