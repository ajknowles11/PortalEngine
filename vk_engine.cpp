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
#include <thread>

#include "data_path.h"

#ifdef _DEBUG
bool constexpr bUseValidationLayers = true;
#else
bool constexpr bUseValidationLayers = false;
#endif

void VulkanEngine::init()
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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
	initImgui();

	isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (isInitialized)
	{
		vkDeviceWaitIdle(device);
		mainDeletionQueue.flush();

		for (unsigned int i = 0; i < FRAME_OVERLAP; i++)
		{
			vkDestroyCommandPool(device, frames[i].commandPool, nullptr);

			vkDestroyFence(device, frames[i].renderFence, nullptr);
			vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
			vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);
		}

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

	VK_CHECK(vkResetFences(device, 1, &getCurrentFrame().renderFence));

	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000, getCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));

	VkCommandBuffer const cmd = getCurrentFrame().mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	drawExtent.width = drawImage.imageExtent.width;
	drawExtent.height = drawImage.imageExtent.height;

	VkCommandBufferBeginInfo const cmdBeginInfo = vkInit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkUtil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	drawBackground(cmd);
	
	vkUtil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

	VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

	frameNumber++;
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool shouldQuit = false;

	while (!shouldQuit)
	{
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT)
			{
				shouldQuit = true;
			}

			if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					stopRendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					stopRendering = false;
				}
			}

			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		if (stopRendering)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();

		if (ImGui::Begin("background"))
		{
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

	mainDeletionQueue.pushFunction([this]()
	{
		vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
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
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} };

	globalDescriptorAllocator.initPool(device, 10, sizes);

	DescriptorLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);

	drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

	VkDescriptorImageInfo const imgInfo
	{
		.imageView = drawImage.imageView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};

	VkWriteDescriptorSet const drawImageWrite
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = drawImageDescriptors,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo = &imgInfo
	};
	vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);
}

void VulkanEngine::initPipelines()
{
	initBackgroundPipelines();
	initTrianglePipeline();
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
		vkDestroyPipeline(device, sky.pipeline, nullptr);
		vkDestroyPipeline(device, gradient.pipeline, nullptr);
	});
}

void VulkanEngine::initTrianglePipeline()
{
	VkShaderModule triangleVertShader;
	if (!vkUtil::load_shader_module(data_path("shaders/colored_triangle.vert.spv").c_str(), device, &triangleVertShader))
	{
		std::cerr << "Error loading triangle vertex shader module \n";
	}

	VkShaderModule triangleFragShader;
	if (!vkUtil::load_shader_module(data_path("shaders/colored_triangle.frag.spv").c_str(), device, &triangleFragShader))
	{
		std::cerr << "Error loading triangle fragment shader module \n";
	}

	VkPipelineLayoutCreateInfo const pipelineLayoutInfo = vkInit::pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout));

	PipelineBuilder pipelineBuilder;

	pipelineBuilder.pipelineLayout = trianglePipelineLayout;
	pipelineBuilder.setShaders(triangleVertShader, triangleFragShader);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.disableBlending();
	pipelineBuilder.disableDepthTest();

	pipelineBuilder.setColorAttachmentFormat(drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);

	trianglePipeline = pipelineBuilder.buildPipeline(device);

	vkDestroyShaderModule(device, triangleFragShader, nullptr);
	vkDestroyShaderModule(device, triangleVertShader, nullptr);

	mainDeletionQueue.pushFunction([&]()
	{
		vkDestroyPipelineLayout(device, trianglePipelineLayout, nullptr);
		vkDestroyPipeline(device, trianglePipeline, nullptr);
	});
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

void VulkanEngine::drawBackground(VkCommandBuffer const cmd) const
{
	ComputeEffect const& effect = backgroundEffects[currentBackgroundEffect];

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
	
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout, 0, 1, &drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(static_cast<float>(drawExtent.width) / 16.0f)), static_cast<uint32_t>(std::ceil(static_cast<float>(drawExtent.height) / 16.0f)), 1);
}

void VulkanEngine::drawGeometry(VkCommandBuffer const cmd) const
{
	VkRenderingAttachmentInfo const colorAttachment = vkInit::attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);

	VkRenderingInfo const renderInfo = vkInit::rendering_info(drawExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

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

	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRendering(cmd);
}

void VulkanEngine::drawImgui(VkCommandBuffer const cmd, VkImageView const targetImageView) const
{
	VkRenderingAttachmentInfo const colorAttachment = vkInit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo const renderInfo = vkInit::rendering_info(swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

