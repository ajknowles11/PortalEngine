// Inspired heavily by vkbootstrap, but manually implemented for my own understanding.

#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"

unsigned int constexpr FRAME_OVERLAP = 2;

class VulkanEngine
{
public:

	bool isInitialized{ false };
	int frameNumber{ 0 };
	bool stopRendering{ false };
	VkExtent2D windowExtent{ 1920, 1080 };
	std::string windowTitle = "Black Hole-in-One";

	struct GLFWwindow* window{ nullptr };

	static VulkanEngine& get();

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice selectedGPU;
	VkDevice device;
	VkSurfaceKHR surface;

	DescriptorAllocator globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;

	VkPipeline gradientPipeline;
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

	void init();
	void cleanup();
	void draw();
	void run();

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();

	void initDescriptors();

	void initPipelines();
	void initBackgroundPipelines();

	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain() const;

	void drawBackground(VkCommandBuffer cmd) const;
};
