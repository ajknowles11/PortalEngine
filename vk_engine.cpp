#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "glm/gtx/transform.hpp"

#include "data_path.h"

#ifdef _DEBUG
bool constexpr bUseValidationLayers = true;
#else
bool constexpr bUseValidationLayers = false;
#endif

bool is_visible(const RenderObject& obj, const glm::mat4& viewProj) {
	std::array<glm::vec3, 8> const corners
	{
		glm::vec3 { 1, 1, 1 },
		glm::vec3 { 1, 1, -1 },
		glm::vec3 { 1, -1, 1 },
		glm::vec3 { 1, -1, -1 },
		glm::vec3 { -1, 1, 1 },
		glm::vec3 { -1, 1, -1 },
		glm::vec3 { -1, -1, 1 },
		glm::vec3 { -1, -1, -1 },
	};

	glm::mat4 const matrix = viewProj * obj.transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };

	for (int c = 0; c < 8; c++) {
		// project each corner into clip space
		glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
		max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
	}

	// check the clip space box is within the view
	if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
		return false;
	}
	else {
		return true;
	}
}


void MeshNode::draw(glm::mat4 const& topMatrix, DrawContext& ctx)
{
	glm::mat4 const nodeMatrix = topMatrix * worldTransform;

	for (auto const& s : mesh->surfaces)
	{
		RenderObject def
		{
			.indexCount = s.count,
			.firstIndex = s.startIndex,
			.indexBuffer = mesh->meshBuffers.indexBuffer.buffer,
			.material = &s.material->data,
			.bounds = s.bounds,
			.transform = nodeMatrix,
			.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress
		};

		if (s.material->data.passType == MaterialPass::Transparent)
		{
			ctx.TransparentSurfaces.push_back(def);
		}
		else
		{
			ctx.OpaqueSurfaces.push_back(def);
		}
	}

	Node::draw(topMatrix, ctx);
}

void GLTFMetallic_Roughness::buildPipelines(VulkanEngine* engine) 
{
	VkShaderModule meshVertShader;
	if (!vkUtil::load_shader_module(data_path("shaders/mesh.vert.spv").c_str(), engine->device, &meshVertShader))
	{
		std::cerr << "Error when building triangle vertex shader module";
	}

	VkShaderModule meshFragShader;
	if (!vkUtil::load_shader_module(data_path("shaders/mesh.frag.spv").c_str(), engine->device, &meshFragShader))
	{
		std::cerr << "Error when building triangle fragment shader module";
	}

	VkPushConstantRange matrixRange
	{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUDrawPushConstants)
	};

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	materialLayout = layoutBuilder.build(engine->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->gpuSceneDataDescriptorLayout, materialLayout };

	VkPipelineLayoutCreateInfo meshLayoutInfo = vkInit::pipeline_layout_create_info();
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->device, &meshLayoutInfo, nullptr, &newLayout));

	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.setShaders(meshVertShader, meshFragShader);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.disableBlending();
	pipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	pipelineBuilder.setColorAttachmentFormat(engine->drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(engine->depthImage.imageFormat);

	pipelineBuilder.pipelineLayout = newLayout;

	opaquePipeline.pipeline = pipelineBuilder.buildPipeline(engine->device);

	pipelineBuilder.enableBlendingAdditive();
	pipelineBuilder.enableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.buildPipeline(engine->device);

	vkDestroyShaderModule(engine->device, meshVertShader, nullptr);
	vkDestroyShaderModule(engine->device, meshFragShader, nullptr);
}

void GLTFMetallic_Roughness::clearResources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
	vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::writeMaterial(VkDevice device, MaterialPass pass, MaterialResources const& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData{};
	matData.passType = pass;
	if (pass == MaterialPass::Transparent)
	{
		matData.pipeline = &transparentPipeline;
	}
	else
	{
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

	writer.clear();
	writer.writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.writeImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.updateSet(device, matData.materialSet);

	return matData;
}

void VulkanEngine::init()
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags constexpr windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	window = SDL_CreateWindow(
		windowTitle.c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		static_cast<int>(windowExtent.width),
		static_cast<int>(windowExtent.height),
		windowFlags
	);

	//import/handle window settings here, such as min size, fullscreen borderless, refresh rate.

	initVulkan();
	initSwapchain();
	initCommands();
	initSyncStructs();
	initDescriptors();
	initPipelines();
	initDefaultData();
	initImgui();

	isInitialized = true;

	mainCamera.velocity = glm::vec3(0.0f);
	mainCamera.position = glm::vec3(30, 0, -85);
	mainCamera.pitch = 0;
	mainCamera.yaw = 0;

	std::string const structurePath = { data_path("assets/structure.glb") };
	auto const structureFile = load_gltf(this, structurePath);

	assert(structureFile.has_value());

	loadedScenes["structure"] = *structureFile;
}

void VulkanEngine::cleanup()
{
	if (isInitialized)
	{
		vkDeviceWaitIdle(device);

		loadedScenes.clear();

		for (unsigned int i = 0; i < FRAME_OVERLAP; i++)
		{
			vkDestroyCommandPool(device, frames[i].commandPool, nullptr);

			vkDestroyFence(device, frames[i].renderFence, nullptr);
			vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
			vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);

			frames[i].deletionQueue.flush();
		}

		metalRoughMaterial.clearResources(device);

		mainDeletionQueue.flush();

		destroySwapchain();

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyDevice(device, nullptr);

		vkb::destroy_debug_utils_messenger(instance, debugMessenger);
		vkDestroyInstance(instance, nullptr);

		SDL_DestroyWindow(window);
	}
}

void VulkanEngine::draw()
{
	VK_CHECK(vkWaitForFences(device, 1, &getCurrentFrame().renderFence, true, 1000000000));

	getCurrentFrame().deletionQueue.flush();
	getCurrentFrame().frameDescriptors.clearPools(device);

	VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().renderFence));

	uint32_t swapchainImageIndex;
	if (VkResult const e = vkAcquireNextImageKHR(device, swapchain, 1000000000, getCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex); e == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resizeRequested = true;
		return;
	}

	VkCommandBuffer const cmd = getCurrentFrame().mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	drawExtent.width = static_cast<uint32_t>(static_cast<float>(std::min(swapchainExtent.width, drawImage.imageExtent.width)) * renderScale);
	drawExtent.height = static_cast<uint32_t>(static_cast<float>(std::min(swapchainExtent.height, drawImage.imageExtent.height)) * renderScale);

	VkCommandBufferBeginInfo const cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkUtil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	drawBackground(cmd);
	
	vkUtil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkUtil::transition_image(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	drawGeometry(cmd);

	vkUtil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkUtil::transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	
	vkUtil::copy_image_to_image(cmd, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);
	
	vkUtil::transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	drawImgui(cmd, swapchainImageViews[swapchainImageIndex]);

	vkUtil::transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo const cmdInfo = vkInit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo const waitInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
	VkSemaphoreSubmitInfo const signalInfo = vkInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

	VkSubmitInfo2 const submit = vkInit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, getCurrentFrame().renderFence));

	VkPresentInfoKHR const presentInfo
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &getCurrentFrame().renderSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &swapchainImageIndex
	};

	if (VkResult const presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo); presentResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resizeRequested = true;
	}

	frameNumber++;
}

void VulkanEngine::updateScene(float const delta)
{
	auto const start = std::chrono::high_resolution_clock::now();

	mainDrawContext.OpaqueSurfaces.clear();
	mainDrawContext.TransparentSurfaces.clear();

	loadedScenes["structure"]->draw(glm::mat4{ 1.0f }, mainDrawContext);

	mainCamera.update(delta);
	glm::mat4 const view = mainCamera.getViewMatrix();
	sceneData.proj = glm::perspective(glm::radians(70.0f), static_cast<float>(drawExtent.width) / static_cast<float>(drawExtent.height), 10000.0f, 0.1f);
	sceneData.proj[1][1] *= -1;
	sceneData.viewProj = sceneData.proj * view;

	sceneData.ambientColor = glm::vec4(0.1f);
	sceneData.sunlightColor = glm::vec4(1.0f);
	sceneData.sunlightDirection = glm::vec4(0, 1, 0.5f, 1.0f);

	auto const end = std::chrono::high_resolution_clock::now();
	auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.meshDrawTime = static_cast<float>(elapsed.count()) / 1000.0f;
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool shouldQuit = false;

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto previousTime = std::chrono::high_resolution_clock::now();

	while (!shouldQuit)
	{
		previousTime = currentTime;
		currentTime = std::chrono::high_resolution_clock::now();
		float elapsed = std::chrono::duration<float>(currentTime - previousTime).count();

		elapsed = std::min(0.1f, elapsed);
		auto const elapsedMs = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - previousTime);

		stats.fps = 1.0f / elapsed;
		stats.frameTime = static_cast<float>(elapsedMs.count()) / 1000.0f;

		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT)
			{
				shouldQuit = true;
			}

			if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_RESIZED)
				{
					resizeRequested = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					stopRendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					stopRendering = false;
				}
			}

			mainCamera.processSDLEvent(e);

			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		if (stopRendering)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (resizeRequested)
		{
			resizeSwapchain();
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();

		ImGui::Begin("Stats");


		ImGui::Text("%f fps", static_cast<double>(stats.fps));
		ImGui::Text("frame time %f ms", static_cast<double>(stats.frameTime));
		ImGui::Text("draw time %f ms", static_cast<double>(stats.meshDrawTime));
		ImGui::Text("update time %f ms", static_cast<double>(stats.sceneUpdateTime));
		ImGui::Text("triangles %i", stats.triangleCount);
		ImGui::Text("draws %i", stats.drawCallCount);

		ImGui::End();

		if (ImGui::Begin("background"))
		{
			ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.0f);

			ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

			ImGui::Text("Selected effect: %s", selected.name.c_str());

			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, static_cast<int>(backgroundEffects.size() - 1));

			ImGui::InputFloat4("data1", reinterpret_cast<float*>(&selected.data.data1));
			ImGui::InputFloat4("data2", reinterpret_cast<float*>(&selected.data.data2));
			ImGui::InputFloat4("data3", reinterpret_cast<float*>(&selected.data.data3));
			ImGui::InputFloat4("data4", reinterpret_cast<float*>(&selected.data.data4));

			ImGui::End();
		}

		ImGui::Render();

		updateScene(elapsed);

		draw();
	}
}

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
	VK_CHECK(vkResetFences(device, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	VkCommandBuffer const cmd = immCommandBuffer;

	VkCommandBufferBeginInfo const cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo const cmdInfo = vkInit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 const submit = vkInit::submit_info(&cmdInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, immFence));

	VK_CHECK(vkWaitForFences(device, 1, &immFence, true, 9999999999));
}

void VulkanEngine::initVulkan()
{
	vkb::InstanceBuilder builder;

	auto instRet = builder.set_app_name(windowTitle.c_str())
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

	vkb::Instance vkbInst = instRet.value();

	instance = vkbInst.instance;
	debugMessenger = vkbInst.debug_messenger;

	SDL_Vulkan_CreateSurface(window, instance, &surface);

	VkPhysicalDeviceVulkan13Features features
	{
		.synchronization2 = true,
		.dynamicRendering = true
	};

	VkPhysicalDeviceVulkan12Features features12
	{
		.descriptorIndexing = true,
		.bufferDeviceAddress = true
	};

	vkb::PhysicalDeviceSelector selector{ vkbInst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_surface(surface)
		.select()
		.value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	device = vkbDevice.device;
	selectedGPU = physicalDevice.physical_device;

	graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo const allocatorInfo
	{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = selectedGPU,
		.device = device,
		.instance = instance
	};
	vmaCreateAllocator(&allocatorInfo, &allocator);

	mainDeletionQueue.pushFunction([&]()
	{
		vmaDestroyAllocator(allocator);
	});
}

void VulkanEngine::initSwapchain()
{
	createSwapchain(windowExtent.width, windowExtent.height);

	VkExtent3D const drawImageExtent = { windowExtent.width, windowExtent.height, 1 };

	drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo const imgCreateInfo = vkInit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent);

	VmaAllocationCreateInfo constexpr imgAllocInfo
	{
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	vmaCreateImage(allocator, &imgCreateInfo, &imgAllocInfo, &drawImage.image, &drawImage.allocation, nullptr);

	VkImageViewCreateInfo const imgViewCreateInfo = vkInit::image_view_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device, &imgViewCreateInfo, nullptr, &drawImage.imageView));

	depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo const dImgCreateInfo = vkInit::image_create_info(depthImage.imageFormat, depthImageUsages, drawImageExtent);

	vmaCreateImage(allocator, &dImgCreateInfo, &imgAllocInfo, &depthImage.image, &depthImage.allocation, nullptr);

	VkImageViewCreateInfo const dImgViewCreateInfo = vkInit::image_view_create_info(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(device, &dImgViewCreateInfo, nullptr, &depthImage.imageView));

	mainDeletionQueue.pushFunction([this]()
	{
		vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);

		vkDestroyImageView(device, depthImage.imageView, nullptr);
		vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
	});
}

void VulkanEngine::initCommands()
{
	VkCommandPoolCreateInfo const commandPoolInfo = vkInit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (unsigned int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));

		VkCommandBufferAllocateInfo const cmdAllocInfo = vkInit::command_buffer_allocate_info(frames[i].commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].mainCommandBuffer));
	}

	VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool));

	VkCommandBufferAllocateInfo const cmdAllocInfo = vkInit::command_buffer_allocate_info(immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immCommandBuffer));

	mainDeletionQueue.pushFunction([this]()
	{
		vkDestroyCommandPool(device, immCommandPool, nullptr);
	});
}

void VulkanEngine::initSyncStructs()
{
	VkFenceCreateInfo const fenceCreateInfo = vkInit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo const semaphoreCreateInfo = vkInit::semaphore_create_info();

	for (unsigned int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));
	}

	VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));
	mainDeletionQueue.pushFunction([this]() {vkDestroyFence(device, immFence, nullptr); });
}

void VulkanEngine::initDescriptors()
{
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1} };

	globalDescriptorAllocator.init(device, 10, sizes);

	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		gpuSceneDataDescriptorLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		singleImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

	DescriptorWriter writer;
	writer.writeImage(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer.updateSet(device, drawImageDescriptors);

	for (uint32_t i = 0; i < FRAME_OVERLAP; i++)
	{
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		frames[i].frameDescriptors = DescriptorAllocatorGrowable{};
		frames[i].frameDescriptors.init(device, 1000, frameSizes);

		mainDeletionQueue.pushFunction([&, i]()
		{
			frames[i].frameDescriptors.destroyPools(device);
		});
	}

	mainDeletionQueue.pushFunction([&]() 
	{
		vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, singleImageDescriptorLayout, nullptr);
		globalDescriptorAllocator.destroyPools(device);
	});
}

void VulkanEngine::initPipelines()
{
	initBackgroundPipelines();

	metalRoughMaterial.buildPipelines(this);
}

void VulkanEngine::initBackgroundPipelines()
{
	VkPushConstantRange pushConstant
	{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(ComputePushConstants)
	};

	VkPipelineLayoutCreateInfo const computeLayout
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.setLayoutCount = 1,
		.pSetLayouts = &drawImageDescriptorLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstant
	};
	VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &gradientPipelineLayout));
	
	VkShaderModule gradientShader;
	if (!vkUtil::load_shader_module(data_path("shaders/gradient.comp.spv").c_str(), device, &gradientShader))
	{
		std::cerr << "Error loading gradient shader \n";
	}
	VkShaderModule skyShader;
	if (!vkUtil::load_shader_module(data_path("shaders/sky.comp.spv").c_str(), device, &skyShader))
	{
		std::cerr << "Error loading sky shader \n";
	}

	VkPipelineShaderStageCreateInfo stageInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = gradientShader,
		.pName = "main"
	};

	VkComputePipelineCreateInfo computePipelineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.stage = stageInfo,
		.layout = gradientPipelineLayout
	};

	ComputeEffect gradient
	{
		.name = "gradient",
		.layout = gradientPipelineLayout,
		.data = {}
	};

	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky
	{
		.name = "sky",
		.layout = gradientPipelineLayout,
		.data = {}
	};

	sky.data.data1 = glm::vec4(0.1f, 0.2f, 0.4f, 0.97f);

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	backgroundEffects.push_back(gradient);
	backgroundEffects.push_back(sky);

	vkDestroyShaderModule(device, gradientShader, nullptr);
	vkDestroyShaderModule(device, skyShader, nullptr);

	mainDeletionQueue.pushFunction([&]()
	{
		vkDestroyPipelineLayout(device, gradientPipelineLayout, nullptr);
		vkDestroyPipeline(device, backgroundEffects[0].pipeline, nullptr);
		vkDestroyPipeline(device, backgroundEffects[1].pipeline, nullptr);
	});
}

void VulkanEngine::initDefaultData()
{
	uint32_t const white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	whiteImage = createImage(&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t const gray = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	grayImage = createImage(&gray, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t const black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
	blackImage = createImage(&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);
	
	uint32_t const magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, static_cast<size_t>(16 * 16) > pixels{};
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	errorCheckerboardImage = createImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(device, &samplerCreateInfo, nullptr, &defaultSamplerNearest);

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(device, &samplerCreateInfo, nullptr, &defaultSamplerLinear);

	mainDeletionQueue.pushFunction([&]() 
	{
		vkDestroySampler(device, defaultSamplerNearest, nullptr);
		vkDestroySampler(device, defaultSamplerLinear, nullptr);

		destroyImage(whiteImage);
		destroyImage(grayImage);
		destroyImage(blackImage);
		destroyImage(errorCheckerboardImage);
	});

	GLTFMetallic_Roughness::MaterialResources materialResources;
	materialResources.colorImage = whiteImage;
	materialResources.colorSampler = defaultSamplerLinear;
	materialResources.metalRoughImage = whiteImage;
	materialResources.metalRoughSampler = defaultSamplerLinear;

	AllocatedBuffer materialConstants = createBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = static_cast<GLTFMetallic_Roughness::MaterialConstants*>(materialConstants.allocation->GetMappedData());
	sceneUniformData->colorFactors = glm::vec4{ 1,1,1,1 };
	sceneUniformData->metal_rough_factors = glm::vec4{ 1, 0.5f, 0, 0 };

	mainDeletionQueue.pushFunction([=, this]()
	{
		destroyBuffer(materialConstants);
	});

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.writeMaterial(device, MaterialPass::MainColor, materialResources, globalDescriptorAllocator);
}

void VulkanEngine::initImgui()
{
	VkDescriptorPoolSize poolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo const poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
		.pPoolSizes = poolSizes
	};

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiPool));

	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(window);

	VkPipelineRenderingCreateInfoKHR const imguiPipelineInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		.pNext = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchainImageFormat
	};

	ImGui_ImplVulkan_InitInfo initInfo
	{
		.Instance = instance,
		.PhysicalDevice = selectedGPU,
		.Device = device,
		.QueueFamily = graphicsQueueFamily,
		.Queue = graphicsQueue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.UseDynamicRendering = true,
		.PipelineRenderingCreateInfo = imguiPipelineInfo
	};
	ImGui_ImplVulkan_Init(&initInfo);

	immediateSubmit([&](VkCommandBuffer cmd) {ImGui_ImplVulkan_CreateFontsTexture(); });

	mainDeletionQueue.pushFunction([=, this]()
	{
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(device, imguiPool, nullptr);
	});
}

void VulkanEngine::createSwapchain(uint32_t const width, uint32_t const height)
{
	vkb::SwapchainBuilder swapchainBuilder{ selectedGPU, device, surface };

	swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.set_desired_format(VkSurfaceFormatKHR{ .format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	swapchainExtent = vkbSwapchain.extent;

	swapchain = vkbSwapchain.swapchain;
	swapchainImages = vkbSwapchain.get_images().value();
	swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroySwapchain() const
{
	vkDestroySwapchainKHR(device, swapchain, nullptr);

	for (size_t i = 0; i < swapchainImageViews.size(); i++)
	{
		vkDestroyImageView(device, swapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::resizeSwapchain()
{
	vkDeviceWaitIdle(device);

	destroySwapchain();

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	windowExtent.width = w;
	windowExtent.height = h;

	createSwapchain(windowExtent.width, windowExtent.height);

	resizeRequested = false;
}

void VulkanEngine::drawBackground(VkCommandBuffer const cmd) const
{
	ComputeEffect const& effect = backgroundEffects[currentBackgroundEffect];

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
	
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout, 0, 1, &drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(drawExtent.width) / 16.0f)), static_cast<uint32_t>(std::ceil(static_cast<float>(drawExtent.height) / 16.0f)), 1);
}

void VulkanEngine::drawGeometry(VkCommandBuffer const cmd)
{
	stats.drawCallCount = 0;
	stats.triangleCount = 0;
	auto const start = std::chrono::high_resolution_clock::now();

	std::vector<uint32_t> opaqueDraws;
	opaqueDraws.reserve(mainDrawContext.OpaqueSurfaces.size());

	for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) 
	{
		if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewProj))
		{
			opaqueDraws.push_back(i);
		}
	}

	// sort the opaque surfaces by material and mesh
	std::ranges::sort(opaqueDraws, [&](const auto& iA, const auto& iB) 
	{
		const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
		if (A.material == B.material) 
		{
			return A.indexBuffer < B.indexBuffer;
		}
		else 
		{
			return A.material < B.material;
		}
	});

	VkRenderingAttachmentInfo const colorAttachment = vkInit::attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
	VkRenderingAttachmentInfo const depthAttachment = vkInit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo const renderInfo = vkInit::rendering_info(windowExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	AllocatedBuffer const gpuSceneDataBuffer = createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	getCurrentFrame().deletionQueue.pushFunction([=, this]()
		{
			destroyBuffer(gpuSceneDataBuffer);
		});

	GPUSceneData* sceneUniformData = static_cast<GPUSceneData*>(gpuSceneDataBuffer.allocation->GetMappedData());
	*sceneUniformData = sceneData;

	VkDescriptorSet globalDescriptor = getCurrentFrame().frameDescriptors.allocate(device, gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.updateSet(device, globalDescriptor);

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](RenderObject const& object)
	{
		if (object.material != lastMaterial)
		{
			lastMaterial = object.material;
			if (object.material->pipeline != lastPipeline)
			{
				lastPipeline = object.material->pipeline;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->pipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);

				VkViewport const viewport
				{
					.x = 0.0f,
					.y = 0.0f,
					.width = static_cast<float>(drawExtent.width),
					.height = static_cast<float>(drawExtent.height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f
				};
				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D const scissor
				{
					.offset
					{
						.x = 0,
						.y = 0
					},
					.extent
					{
						.width = drawExtent.width,
						.height = drawExtent.height
					}
				};
				vkCmdSetScissor(cmd, 0, 1, &scissor);
			}
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->layout, 1, 1, &object.material->materialSet, 0, nullptr);
		}

		if (object.indexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = object.indexBuffer;
			vkCmdBindIndexBuffer(cmd, object.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}

		GPUDrawPushConstants pushConstants{};
		pushConstants.worldMatrix = object.transform;
		pushConstants.vertexBuffer = object.vertexBufferAddress;

		vkCmdPushConstants(cmd, object.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(cmd, object.indexCount, 1, object.firstIndex, 0, 0);

		stats.drawCallCount++;
		stats.triangleCount += static_cast<int>(object.indexCount) / 3;
	};

	for (auto r : opaqueDraws)
	{
		draw(mainDrawContext.OpaqueSurfaces[r]);
	}
	for (auto r : mainDrawContext.TransparentSurfaces)
	{
		draw(r);
	}

	vkCmdEndRendering(cmd);

	auto const end = std::chrono::high_resolution_clock::now();
	auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.meshDrawTime = static_cast<float>(elapsed.count()) / 1000.0f;
}

void VulkanEngine::drawImgui(VkCommandBuffer const cmd, VkImageView const targetImageView) const
{
	VkRenderingAttachmentInfo const colorAttachment = vkInit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo const renderInfo = vkInit::rendering_info(swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

AllocatedBuffer VulkanEngine::createBuffer(size_t const allocSize, VkBufferUsageFlags const usage, VmaMemoryUsage const memoryUsage) const
{
	// allocate buffer
	VkBufferCreateInfo const bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.size = allocSize,
		.usage = usage
	};

	VmaAllocationCreateInfo const vmaAllocInfo
	{
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = memoryUsage
	};

	AllocatedBuffer newBuffer{};

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void VulkanEngine::destroyBuffer(AllocatedBuffer const& buffer) const
{
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> const indices, std::span<Vertex> const vertices) const
{
	size_t const vertexBufferSize = vertices.size() * sizeof(Vertex);
	size_t const indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface{};

	newSurface.vertexBuffer = createBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo const deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);

	newSurface.indexBuffer = createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer const staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData();

	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);

	immediateSubmit([&](VkCommandBuffer const cmd)
	{
		VkBufferCopy const vertexCopy
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = vertexBufferSize
		};
		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy const indexCopy
		{
			.srcOffset = vertexBufferSize,
			.dstOffset = 0,
			.size = indexBufferSize
		};
		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
	});

	destroyBuffer(staging);

	return newSurface;
}

AllocatedImage VulkanEngine::createImage(VkExtent3D const size, VkFormat const format, VkImageUsageFlags const usage, bool const mipmapped) const
{
	AllocatedImage newImage
	{
		.imageExtent = size,
		.imageFormat = format
	};

	VkImageCreateInfo imgInfo = vkInit::image_create_info(format, usage, size);
	if (mipmapped)
	{
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	VmaAllocationCreateInfo constexpr allocInfo
	{
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};

	VK_CHECK(vmaCreateImage(allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT)
	{
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo viewInfo = vkInit::image_view_create_info(format, newImage.image, aspectFlag);
	viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanEngine::createImage(void const* data, VkExtent3D const size, VkFormat const format, VkImageUsageFlags const usage, bool const mipmapped) const
{
	size_t const dataSize = static_cast<size_t>(size.depth) * static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4;
	AllocatedBuffer const uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadBuffer.info.pMappedData, data, dataSize);

	AllocatedImage const newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	immediateSubmit([&](VkCommandBuffer const cmd)
	{
		vkUtil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy const copyRegion
		{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.imageExtent = size
		};

		vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		if (mipmapped)
		{
			vkUtil::generate_mipmaps(cmd, newImage.image, VkExtent2D{ newImage.imageExtent.width, newImage.imageExtent.height });
		}
		else
		{
			vkUtil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	});

	destroyBuffer(uploadBuffer);

	return newImage;
}


void VulkanEngine::destroyImage(AllocatedImage const& img) const
{
	vkDestroyImageView(device, img.imageView, nullptr);
	vmaDestroyImage(allocator, img.image, img.allocation);
}

