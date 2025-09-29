#pragma once

#include <unordered_map>
#include <filesystem>

#include "vk_descriptors.h"
#include "vk_types.h"

class VulkanEngine;

struct GLTFMaterial
{
	MaterialInstance data;
};

struct Bounds
{
	glm::vec3 origin;
	float sphereRadius;
	glm::vec3 extents;
};

struct GeoSurface
{
	uint32_t startIndex;
	uint32_t count;
	Bounds bounds;
	std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
	std::string name;

	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

struct LoadedGLTF : public IRenderable
{
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
	std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
	std::unordered_map<std::string, AllocatedImage> images;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

	std::vector<std::shared_ptr<Node>> topNodes;

	std::vector<VkSampler> samplers;

	DescriptorAllocatorGrowable descriptorPool;

	AllocatedBuffer materialDataBuffer;

	VulkanEngine* creator;

	virtual ~LoadedGLTF() { clearAll(); }

	void draw(glm::mat4 const& topMatrix, DrawContext& ctx) override;

private:

	void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* engine, std::string_view filePath);

std::optional<std::shared_ptr<LoadedGLTF>> load_pscn(VulkanEngine* engine, std::string_view filePath);