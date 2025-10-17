#include "vk_loader.h"

#include "stb_image.h"
#include <chrono>
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_types.h"
#include "vk_pipelines.h"
#include <glm/gtx/quaternion.hpp>

#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"

// could be handled on GPU, maybe in VulkanEngine::CreateImage instead
void premultiply_alpha(unsigned char* data, VkExtent3D const size)
{
	size_t const dataSize = static_cast<size_t>(size.depth) * static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4;

	for (int i = 0; i < dataSize; i += 4)
	{
		uint16_t r = static_cast<uint16_t>(data[i]);
		uint16_t g = static_cast<uint16_t>(data[i + 1]);
		uint16_t b = static_cast<uint16_t>(data[i + 2]);
		uint16_t a = static_cast<uint16_t>(data[i + 3]);

		r = r * a / 255;
		g = g * a / 255;
		b = b * a / 255;

		data[i] = static_cast<unsigned char>(r);
		data[i + 1] = static_cast<unsigned char>(g);
		data[i + 2] = static_cast<unsigned char>(b);
	}
}

std::optional<AllocatedImage> load_image(VulkanEngine const* engine, fastgltf::Asset& asset, fastgltf::Image& image, std::filesystem::path directory)
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
				assert(filePath.uri.isLocalPath()); // We're only capable of loading local files.\

				std::string const fixedPath = std::move(directory).string() + "/" + std::string(filePath.uri.path());

				unsigned char* data = stbi_load(fixedPath.c_str(), &width, &height, &nrChannels, 4);
				if (data)
				{
					VkExtent3D const imageSize
					{
						.width = static_cast<uint32_t>(width),
						.height = static_cast<uint32_t>(height),
						.depth = 1
					};

					premultiply_alpha(data, imageSize);
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

					premultiply_alpha(data, imageSize);
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

							premultiply_alpha(data, imageSize);
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
	VkPhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(engine->selectedGPU, &deviceProperties);

	std::cout << "Loading glTF: " << filePath << "\n";

	auto const startTime = std::chrono::high_resolution_clock::now();
	auto lastTime = startTime;

	auto scene = std::make_shared<LoadedGLTF>();
	scene->creator = engine;
	LoadedGLTF& file = *scene.get();

	fastgltf::Parser parser{};

	auto constexpr gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filePath);

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
	lastTime = currentTime;

	std::cout << "> file loaded in " << elapsed << "." << std::endl;

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

	currentTime = std::chrono::high_resolution_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
	lastTime = currentTime;

	std::cout << "> glTF loaded in " << elapsed << "." << std::endl;

	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{ {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} };

	if (gltf.samplers.empty())
	{
		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy,
			.minLod = 0,
			.maxLod = VK_LOD_CLAMP_NONE,
		};

		VkSampler newSampler;
		vkCreateSampler(engine->device, &samplerInfo, nullptr, &newSampler);

		file.samplers.push_back(newSampler);
	}

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

	currentTime = std::chrono::high_resolution_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
	lastTime = currentTime;

	std::cout << "> samplers loaded in " << elapsed << "." << std::endl;

	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	// load all textures
	for (fastgltf::Image& image : gltf.images) {
		std::optional<AllocatedImage> img = load_image(engine, gltf, image, path.parent_path());

		if (img.has_value()) {
			images.push_back(*img);
			// We should require images to be uniquely named
			// But for now I'll just warn and add parentheses. 
			// This is really slow but don't give me bad gltf files.
			if (file.images.contains(image.name.c_str()))
			{
				std::string newName = image.name.c_str();
				newName.append(" (1)");
				int i = 1;
				while (file.images.contains(newName))
				{
					// Remove end chars to give room for parentheses
					newName.pop_back();
					for (int j = 0; j <= i / 10; j++)
					{
						newName.pop_back();
					}
					++i;
					newName.append(std::to_string(i) + ")");
				}
				std::cerr << "Warning: image name " + image.name + " already in use. Renaming to ";
				std::cerr << newName + ".\n";
				image.name = newName;
			}
			file.images[image.name.c_str()] = *img;
		}
		else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine->errorCheckerboardImage);
			std::cout << "gltf failed to load texture " << image.name << std::endl;
		}
	}

	currentTime = std::chrono::high_resolution_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
	lastTime = currentTime;

	std::cout << "> textures loaded in " << elapsed << "." << std::endl;

	if (!gltf.materials.empty())
	{
		file.descriptorPool.init(engine->device, static_cast<uint32_t>(gltf.materials.size()), sizes);
		file.materialDataBuffer = engine->createBuffer(sizeof(PBRMaterial::MaterialConstants) * gltf.materials.size(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		int data_index = 0;
		auto sceneMaterialConstants = static_cast<PBRMaterial::MaterialConstants*>(file.materialDataBuffer.info.pMappedData);

		for (fastgltf::Material& mat : gltf.materials)
		{
			auto newMat = std::make_shared<GLTFMaterial>();
			materials.push_back(newMat);
			file.materials[mat.name.c_str()] = newMat;

			PBRMaterial::MaterialConstants constants{};
			constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
			constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
			constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
			constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

			constants.metalRoughFactors.x = mat.pbrData.metallicFactor;
			constants.metalRoughFactors.y = mat.pbrData.roughnessFactor;
			// write material parameters to buffer
			sceneMaterialConstants[data_index] = constants;

			auto passType = MaterialPass::MainColor;
			if (mat.alphaMode == fastgltf::AlphaMode::Blend)
			{
				passType = MaterialPass::Transparent;
			}

			PBRMaterial::MaterialResources materialResources{};
			// default the material textures
			materialResources.albedoImage = engine->whiteImage;
			materialResources.albedoSampler = engine->defaultSamplerLinear;
			materialResources.normalImage = engine->defaultNormalImage;
			materialResources.normalSampler = engine->defaultSamplerLinear;
			materialResources.metalRoughAOImage = engine->whiteImage;
			materialResources.metalRoughAOSampler = engine->defaultSamplerLinear;

			// set the uniform buffer for the material data
			materialResources.dataBuffer = file.materialDataBuffer.buffer;
			materialResources.dataBufferOffset = data_index * sizeof(PBRMaterial::MaterialConstants);
			// grab textures from gltf file
			if (mat.pbrData.baseColorTexture.has_value())
			{
				size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				size_t sample = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.has_value() ?
					gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value() : 0;

				materialResources.albedoImage = images[img];
				materialResources.albedoSampler = file.samplers[sample];
			}
			if (mat.normalTexture.has_value())
			{
				size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
				size_t sample = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.has_value() ?
					gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value() : 0;

				materialResources.normalImage = images[img];
				materialResources.normalSampler = file.samplers[sample];
			}
			if (mat.pbrData.metallicRoughnessTexture.has_value())
			{
				size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
				size_t sample = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.has_value() ?
					gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value() : 0;

				materialResources.metalRoughAOImage = images[img];
				materialResources.metalRoughAOSampler = file.samplers[sample];
			}
			// build material
			newMat->data = engine->pbrMaterial.writeMaterial(engine->device, passType, materialResources, file.descriptorPool);

			data_index++;
		}
	}
	
	currentTime = std::chrono::high_resolution_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
	lastTime = currentTime;

	std::cout << "> materials loaded in " << elapsed << "." << std::endl;

	// use the same vectors for all meshes so that the memory doesn't reallocate as
	// often
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh& mesh : gltf.meshes) 
	{
		auto newMesh = std::make_shared<MeshAsset>();
		meshes.push_back(newMesh);
		// We should require meshes to be uniquely named
		// But for now I'll just warn and add parentheses. 
		// This is really slow but don't give me bad gltf files.
		if (file.meshes.contains(mesh.name.c_str()))
		{
			std::string newName = mesh.name.c_str();
			newName.append(" (1)");
			int i = 1;
			while (file.meshes.contains(newName))
			{
				// Remove end chars to give room for parentheses
				newName.pop_back();
				for (int j = 0; j <= i / 10; j++)
				{
					newName.pop_back();
				}
				++i;
				newName.append(std::to_string(i) + ")");
			}
			std::cerr << "Warning: mesh name " + mesh.name + " already in use. Renaming to ";
			std::cerr << newName + ".\n";
			mesh.name = newName;
		}
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
			fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
			{
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

			auto tangents = p.findAttribute("TANGENT");
			if (tangents != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*tangents).second],
					[&](glm::vec4 const v, size_t const index)
					{
						vertices[initialVtx + index].surfaceTangent = v;
					});
			}
			else
			{
				// calculate tangents ourselves
				// perhaps should just use mikktspace instead
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 const v, size_t const index)
					{
						if (index % 3 == 0 && index + 2 < posAccessor.count)
						{
							Vertex& v0 = vertices[initialVtx + index];
							Vertex& v1 = vertices[initialVtx + index + 1];
							Vertex& v2 = vertices[initialVtx + index + 2];

							glm::vec3 const e1 = v1.position - v0.position;
							glm::vec3 const e2 = v2.position - v0.position;
							glm::vec2 const dUV1 = { v1.uv_x - v0.uv_x, v1.uv_y - v0.uv_y };
							glm::vec2 const dUV2 = { v2.uv_x - v0.uv_x, v2.uv_y - v0.uv_y };

							float const det = (dUV1.x * dUV2.y - dUV2.x * dUV1.y);
							if (det == 0)
							{
								// Degenerate triangle
								v0.surfaceTangent = { 1.0f, 0, 0, 0 };
								v1.surfaceTangent = { 1.0f, 0, 0, 0 };
								v2.surfaceTangent = { 1.0f, 0, 0, 0 };
							}
							else
							{
								float const invDet = 1.0f / det;
								glm::vec3 const tangent = invDet * (dUV2.y * e1 - dUV1.y * e2);
								// This tangent is not actually orthogonal to normal vector, since vertices have their own normals not necessarily equal to surface normal.
								// We will perform Gram-Schmidt in vertex shader to fix this.
								v0.surfaceTangent = glm::vec4(tangent, 0);
								v1.surfaceTangent = glm::vec4(tangent, 0);
								v2.surfaceTangent = glm::vec4(tangent, 0);
							}
						}
					});
			}

			if (p.materialIndex.has_value())
			{
				newSurface.material = materials[p.materialIndex.value()];
			}
			else if (!materials.empty())
			{
				newSurface.material = materials[0];
			}
			else
			{
				newSurface.material = engine->defaultMaterial;
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

	currentTime = std::chrono::high_resolution_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);
	lastTime = currentTime;

	std::cout << "> meshes loaded in " << elapsed << "." << std::endl;

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

	currentTime = std::chrono::high_resolution_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

	std::cout << "> scene ready in " << elapsed << " (total)." << std::endl;
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

std::optional<AllocatedImage> load_cubemap_from_hdri(VulkanEngine* engine, std::string_view filePath)
{
	static int const cubeMapSize = 512;

	int width, height, nrChannels;
	float* data = stbi_loadf(filePath.data(), &width, &height, &nrChannels, 0);
	if (data)
	{
		VkPhysicalDeviceProperties deviceProperties{};
		vkGetPhysicalDeviceProperties(engine->selectedGPU, &deviceProperties);

		AllocatedImage hdrImage{};
		{
			VkExtent3D const imageSize
			{
				.width = static_cast<uint32_t>(width),
				.height = static_cast<uint32_t>(height),
				.depth = 1
			};

			char* newData = new char[2 * 4 * width * height]();
			for (size_t p = 0; p < (size_t)width * (size_t)height; p++)
			{
				newData[8 * p + 0] = reinterpret_cast<char*>(data)[12 * p + 2];
				newData[8 * p + 1] = reinterpret_cast<char*>(data)[12 * p + 3];
				newData[8 * p + 2] = reinterpret_cast<char*>(data)[12 * p + 6];
				newData[8 * p + 3] = reinterpret_cast<char*>(data)[12 * p + 7];
				newData[8 * p + 4] = reinterpret_cast<char*>(data)[12 * p + 10];
				newData[8 * p + 5] = reinterpret_cast<char*>(data)[12 * p + 11];
			}

			hdrImage = engine->createImage(newData, imageSize, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);
			delete[] newData;
		}
		
		AllocatedImage cubemapImage{};
		{
			VkExtent3D cubemapExtent
			{
				.width = 512,
				.height = 512,
				.depth = 6
			};
			cubemapImage = engine->createImage(cubemapExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		}
		
		VkSampler hdriSampler;
		{
			VkSamplerCreateInfo samplerInfo
			{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.pNext = nullptr,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.anisotropyEnable = VK_TRUE,
				.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy,
				.minLod = 0,
				.maxLod = VK_LOD_CLAMP_NONE,
			};
			vkCreateSampler(engine->device, &samplerInfo, nullptr, &hdriSampler);
		}

		VkDescriptorSet computeDescriptor;
		VkDescriptorSetLayout computeLayout;
		DescriptorAllocatorGrowable alloc;
		{
			std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} };
			alloc.init(engine->device, 1, sizes);

			DescriptorLayoutBuilder builder;
			builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			builder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			computeLayout = builder.build(engine->device, VK_SHADER_STAGE_COMPUTE_BIT);
			computeDescriptor = alloc.allocate(engine->device, computeLayout, nullptr);
		}

		VkPipeline computePipeline;
		VkPipelineLayout computePipelineLayout;
		{
			VkPipelineLayoutCreateInfo const computeLayoutInfo
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.setLayoutCount = 1,
				.pSetLayouts = &computeLayout
			};
			VK_CHECK(vkCreatePipelineLayout(engine->device, &computeLayoutInfo, nullptr, &computePipelineLayout));

			VkShaderModule cubemapShader;
			if (!vkUtil::load_shader_module((engine->baseAppPath + "shaders/make_cubemap.comp.spv").c_str(), engine->device, &cubemapShader))
			{
				std::cerr << "Error when building cubemap compute shader module";
			}

			VkPipelineShaderStageCreateInfo stageInfo
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = nullptr,
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.module = cubemapShader,
				.pName = "main"
			};

			VkComputePipelineCreateInfo computePipelineCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
				.pNext = nullptr,
				.stage = stageInfo,
				.layout = computePipelineLayout
			};

			VK_CHECK(vkCreateComputePipelines(engine->device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline));
		
			vkDestroyShaderModule(engine->device, cubemapShader, nullptr);
		}

		// render to cubemap here
		{
			DescriptorWriter writer;
			writer.writeImage(0, hdrImage.imageView, hdriSampler, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			writer.writeImage(1, cubemapImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer.updateSet(engine->device, computeDescriptor);

			engine->immediateSubmit([&](VkCommandBuffer cmd)
				{
					vkUtil::transition_image(cmd, cubemapImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptor, 0, nullptr);

					vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(cubeMapSize) / 16.0f)), static_cast<uint32_t>(std::ceil(static_cast<float>(cubeMapSize) / 16.0f)), 6);

					vkUtil::transition_image(cmd, cubemapImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				});
		}

		alloc.destroyPools(engine->device);
		vkDestroyDescriptorSetLayout(engine->device, computeLayout, nullptr);
		vkDestroyPipelineLayout(engine->device, computePipelineLayout, nullptr);
		vkDestroyPipeline(engine->device, computePipeline, nullptr);
		vkDestroySampler(engine->device, hdriSampler, nullptr);
		engine->destroyImage(hdrImage);

		stbi_image_free(data);

		return cubemapImage;
	}
	else
	{
		std::cerr << "Failed to load HDRI image at " << filePath << "\n";
		return std::nullopt;
	}
}

std::optional<std::shared_ptr<LoadedGLTF>> load_pscn(VulkanEngine* engine, std::string_view filePath) 
{
	return std::nullopt;
}