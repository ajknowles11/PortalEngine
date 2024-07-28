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

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
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

enum class MaterialPass :uint8_t
{
	MainColor,
	Transparent,
	Other
};

struct MaterialPipeline 
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance 
{
	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
	MaterialPass passType;
};

struct DrawContext;

class IRenderable 
{
	virtual void draw(glm::mat4 const& topMatrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable
{
	std::weak_ptr<Node> parent;
	std::vector<std::shared_ptr<Node>> children;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	void refreshTransform(glm::mat4 const& parentMatrix)
	{
		worldTransform = parentMatrix * localTransform;
		for (auto c : children)
		{
			c->refreshTransform(worldTransform);
		}
	}

	virtual void draw(glm::mat4 const& topMatrix, DrawContext& ctx)
	{
		for (auto& c : children)
		{
			c->draw(topMatrix, ctx);
		}
	}
};

struct GPUSceneData
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewProj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection;
	glm::vec4 sunlightColor;
};
