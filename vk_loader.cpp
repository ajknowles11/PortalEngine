#include "vk_loader.h"

#include "stb_image.h"
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"

std::optional<AllocatedImage> load_image(VulkanEngine const* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
	AllocatedImage newImage{};

	int width, height, nrChannels;

	std::visit(
		fastgltf::visitor
		{
			[](auto& arg) {},
			[&](fastgltf::sources::URI& filePath)
			{
				assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
				assert(filePath.uri.isLocalPath()); // We're only capable of loading local files.

				unsigned char* data = stbi_load(filePath.uri.path().data(), &width, &height, &nrChannels, 4);
				if (data)
				{
					VkExtent3D const imageSize
					{
						.width = static_cast<uint32_t>(width),
						.height = static_cast<uint32_t>(height),
						.depth = 1
					};

					newImage = engine->createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::Array& vector)
			{
				unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
					&width, &height, &nrChannels, 4);
				if (data)
				{
					VkExtent3D const imageSize
					{
						.width = static_cast<uint32_t>(width),
						.height = static_cast<uint32_t>(height),
						.depth = 1
					};

					newImage = engine->createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::BufferView& view)
			{
				auto const& bufferView = asset.bufferViews[view.bufferViewIndex];
				auto& buffer = asset.buffers[bufferView.bufferIndex];

				std::visit(fastgltf::visitor
				{
					// We only care about VectorWithMime here, because we
					// specify LoadExternalBuffers, meaning all buffers
					// are already loaded into a vector.
					[](auto& arg) {},
					[&](fastgltf::sources::Array& vector)
					{
						unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
							static_cast<int>(bufferView.byteLength),
							&width, &height, &nrChannels, 4);
						if (data)
						{
							VkExtent3D const imageSize
							{
								.width = static_cast<uint32_t>(width),
								.height = static_cast<uint32_t>(height),
								.depth = 1
							};

							newImage = engine->createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM,
								VK_IMAGE_USAGE_SAMPLED_BIT, true);

							stbi_image_free(data);
						}
					}
				},
				buffer.data);
			}
		},
		image.data);

	// if any of the attempts to load the data failed, we haven't written the image
	// so handle is null
	if (newImage.image == VK_NULL_HANDLE) {
		return {};
	}
	else {
		return newImage;
	}
}

VkFilter extract_filter(fastgltf::Filter const filter)
{
	switch (filter)
	{
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_FILTER_NEAREST;

	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
		return VK_FILTER_LINEAR;
	}
	return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter const filter)
{
	switch (filter)
	{
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* engine, std::string_view filePath)
{
	std::cout << "Loading glTF: " << filePath << "\n";

	auto scene = std::make_shared<LoadedGLTF>();
	scene->creator = engine;
	LoadedGLTF& file = *scene.get();

	fastgltf::Parser parser{};

	auto constexpr gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filePath);

	fastgltf::Asset gltf;

	std::filesystem::path path = filePath;

	auto type = fastgltf::determineGltfFileType(&data);
	if (type == fastgltf::GltfType::glTF)
	{
		auto load = parser.loadGltf(&data, path.parent_path(), gltfOptions);
		if (load)
		{
			gltf = std::move(load.get());
		}
		else
		{
			std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error());
			return {};
		}
	}
	else if (type == fastgltf::GltfType::GLB)
	{
		auto load = parser.loadGltfBinary(&data, path.parent_path(), gltfOptions);
		if (load)
		{
			gltf = std::move(load.get());
		}
		else
		{
			std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error());
			return {};
		}
	}
	else
	{
		std::cerr << "Failed to determine glTF container\n";
		return {};
	}

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{ {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} };

	file.descriptorPool.init(engine->device, static_cast<uint32_t>(gltf.materials.size()), sizes);

	for (fastgltf::Sampler& sampler : gltf.samplers)
	{
		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
			.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			.minLod = 0,
			.maxLod = VK_LOD_CLAMP_NONE,
		};

		VkSampler newSampler;
		vkCreateSampler(engine->device, &samplerInfo, nullptr, &newSampler);

		file.samplers.push_back(newSampler);
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	// load all textures
	for (fastgltf::Image& image : gltf.images) {
		std::optional<AllocatedImage> img = load_image(engine, gltf, image);

		if (img.has_value()) {
			images.push_back(*img);
			file.images[image.name.c_str()] = *img;
		}
		else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine->errorCheckerboardImage);
			std::cout << "gltf failed to load texture " << image.name << std::endl;
		}
	}

	file.materialDataBuffer = engine->createBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	int data_index = 0;
	auto sceneMaterialConstants = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(file.materialDataBuffer.info.pMappedData);

	for (fastgltf::Material& mat : gltf.materials)
	{
		auto newMat = std::make_shared<GLTFMaterial>();
		materials.push_back(newMat);
		file.materials[mat.name.c_str()] = newMat;

		GLTFMetallic_Roughness::MaterialConstants constants{};
		constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
		constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
		constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
		constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

		constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
		constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
		// write material parameters to buffer
		sceneMaterialConstants[data_index] = constants;

		auto passType = MaterialPass::MainColor;
		if (mat.alphaMode == fastgltf::AlphaMode::Blend) 
		{
			passType = MaterialPass::Transparent;
		}

		GLTFMetallic_Roughness::MaterialResources materialResources{};
		// default the material textures
		materialResources.colorImage = engine->whiteImage;
		materialResources.colorSampler = engine->defaultSamplerLinear;
		materialResources.metalRoughImage = engine->whiteImage;
		materialResources.metalRoughSampler = engine->defaultSamplerLinear;

		// set the uniform buffer for the material data
		materialResources.dataBuffer = file.materialDataBuffer.buffer;
		materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);
		// grab textures from gltf file
		if (mat.pbrData.baseColorTexture.has_value()) 
		{
			size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
			size_t sample = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

			materialResources.colorImage = images[img];
			materialResources.colorSampler = file.samplers[sample];
		}
		// build material
		newMat->data = engine->metalRoughMaterial.writeMaterial(engine->device, passType, materialResources, file.descriptorPool);

		data_index++;
	}

	// use the same vectors for all meshes so that the memory doesn't reallocate as
	// often
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh& mesh : gltf.meshes) 
	{
		auto newMesh = std::make_shared<MeshAsset>();
		meshes.push_back(newMesh);
		file.meshes[mesh.name.c_str()] = newMesh;
		newMesh->name = mesh.name;

		// clear the mesh arrays each mesh, we don't want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) 
		{
			GeoSurface newSurface;
			newSurface.startIndex = static_cast<uint32_t>(indices.size());
			newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

			size_t initialVtx = vertices.size();

			// load indexes
			{
				fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
					[&](uint32_t const idx) 
					{
						indices.push_back(idx + static_cast<uint32_t>(initialVtx));
					});
			}

			// load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 const v, size_t const index) 
					{
						Vertex newVtx{};
						newVtx.position = v;
						newVtx.normal = { 1, 0, 0 };
						newVtx.color = glm::vec4{ 1.f };
						newVtx.uv_x = 0;
						newVtx.uv_y = 0;
						vertices[initialVtx + index] = newVtx;
					});
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) 
			{

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
					[&](glm::vec3 const v, size_t const index) 
					{
						vertices[initialVtx + index].normal = v;
					});
			}

			// load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) 
			{

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
					[&](glm::vec2 const v, size_t const index) 
					{
						vertices[initialVtx + index].uv_x = v.x;
						vertices[initialVtx + index].uv_y = v.y;
					});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) 
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
					[&](glm::vec4 const v, size_t const index) 
					{
						vertices[initialVtx + index].color = v;
					});
			}

			if (p.materialIndex.has_value())
			{
				newSurface.material = materials[p.materialIndex.value()];
			}
			else 
			{
				newSurface.material = materials[0];
			}

			glm::vec3 minPos = vertices[initialVtx].position;
			glm::vec3 maxPos = vertices[initialVtx].position;
			for (size_t i = initialVtx; i < vertices.size(); i++) 
			{
				minPos = glm::min(minPos, vertices[i].position);
				maxPos = glm::max(maxPos, vertices[i].position);
			}

			newSurface.bounds.origin = (maxPos + minPos) / 2.f;
			newSurface.bounds.extents = (maxPos - minPos) / 2.f;
			newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);
			newMesh->surfaces.push_back(newSurface);
		}

		newMesh->meshBuffers = engine->uploadMesh(indices, vertices);
	}

	// load all nodes and their meshes
	for (fastgltf::Node& node : gltf.nodes) 
	{
		std::shared_ptr<Node> newNode;

		// find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
		if (node.meshIndex.has_value()) 
		{
			newNode = std::make_shared<MeshNode>();
			dynamic_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
		}
		else {
			newNode = std::make_shared<Node>();
		}

		nodes.push_back(newNode);
		file.nodes[node.name.c_str()];

		std::visit(fastgltf::visitor
			{
				[&](fastgltf::Node::TransformMatrix const matrix) 
				{
					memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
				},
				[&](fastgltf::TRS const transform)
				{
					glm::vec3 const tl(transform.translation[0], transform.translation[1], transform.translation[2]);
					glm::quat const rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
					glm::vec3 const sc(transform.scale[0], transform.scale[1], transform.scale[2]);

					glm::mat4 const tm = glm::translate(glm::mat4(1.f), tl);
					glm::mat4 const rm = glm::toMat4(rot);
					glm::mat4 const sm = glm::scale(glm::mat4(1.f), sc);

					newNode->localTransform = tm * rm * sm;
				}
			},
			node.transform);
	}

	// run loop again to setup transform hierarchy
	for (size_t i = 0; i < gltf.nodes.size(); i++) 
	{
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];

		for (auto& c : node.children) 
		{
			sceneNode->children.push_back(nodes[c]);
			nodes[c]->parent = sceneNode;
		}
	}

	// find the top nodes, with no parents
	for (auto& node : nodes) 
	{
		if (node->parent.lock() == nullptr) 
		{
			file.topNodes.push_back(node);
			node->refreshTransform(glm::mat4{ 1.f });
		}
	}
	return scene;
}

void LoadedGLTF::draw(glm::mat4 const& topMatrix, DrawContext& ctx)
{
	for (auto const& n : topNodes)
	{
		n->draw(topMatrix, ctx);
	}
}

void LoadedGLTF::clearAll()
{
	VkDevice const dv = creator->device;

	descriptorPool.destroyPools(dv);
	creator->destroyBuffer(materialDataBuffer);

	for (auto& [k, v] : meshes) {

		creator->destroyBuffer(v->meshBuffers.indexBuffer);
		creator->destroyBuffer(v->meshBuffers.vertexBuffer);
	}

	for (auto& [k, v] : images) {

		if (v.image == creator->errorCheckerboardImage.image) {
			//dont destroy the default images
			continue;
		}
		creator->destroyImage(v);
	}

	for (auto const& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
	}
}
