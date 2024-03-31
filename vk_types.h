#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#ifdef _DEBUG
#include <iostream>
#define VK_CHECK(x)                                                                   \
    do {                                                                              \
        VkResult err = x;                                                             \
        if (err) {                                                                    \
             std::cerr << "Detected Vulkan error: " << string_VkResult(err) << "\n"; \
            abort();                                                                  \
        }                                                                             \
    } while (0)
#else
#define VK_CHECK(x) x
#endif