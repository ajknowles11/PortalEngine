// Inspired heavily by vkbootstrap, but manually implemented for my own understanding.

#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"

unsigned int constexpr FRAME_OVERLAP = 2;

struct FrameData
{
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	VkSemaphore swapchainSemaphore, renderSemaphore;
	VkFence renderFence;

	DeletionQueue deletionQueue;
};

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect
{
	std::string name;
	VkPipeline pipeline;
	VkPipelineLayout layout;
	ComputePushConstants data;
};

class VulkanEngine
{
public:

	bool isInitialized{ false };
	int frameNumber{ 0 };
	bool stopRendering{ false };
	VkExtent2D windowExtent{ 1920, 1080 };
	std::string windowTitle = "Black Hole-in-One";

	struct SDL_Window* window{ nullptr };

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice selectedGPU;
	VkDevice device;
	VkSurfaceKHR surface;

	DescriptorAllocator globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;

	VkPipelineLayout gradientPipelineLayout;

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;

	AllocatedImage drawImage;
	VkExtent2D drawExtent;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkExtent2D swapchainExtent;

	FrameData frames[FRAME_OVERLAP];

	FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamily;

	DeletionQueue mainDeletionQueue;

	VmaAllocator allocator;

	VkFence immFence;
	VkCommandBuffer immCommandBuffer;
	VkCommandPool immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect = 0;

	VkPipelineLayout trianglePipelineLayout;
	VkPipeline trianglePipeline;

	void init();
	void cleanup();
	void draw();
	void run();

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();

	void initDescriptors();

	void initPipelines();
	void initBackgroundPipelines();
	void initTrianglePipeline();

	void initImgui();

	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain() const;

	void drawBackground(VkCommandBuffer cmd) const;
	void drawGeometry(VkCommandBuffer cmd) const;
	void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
};
