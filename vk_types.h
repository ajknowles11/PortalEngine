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

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void pushFunction(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct AllocatedBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct Vertex
{
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct GPUMeshBuffers
{
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;
};

struct GPUDrawPushConstants
{
	glm::mat4 worldMatrix;
	VkDeviceAddress vertexBuffer;
};
