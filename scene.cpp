#include "scene.h"

#include <fstream>

Scene::Scene(std::string_view const path) 
{
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file.is_open() || file.bad())
        return;

}