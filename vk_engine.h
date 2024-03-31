// Inspired heavily by vkbootstrap, but manually implemented for my own understanding.

#pragma once

#include "vk_types.h"

struct FrameData
{
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	VkSemaphore swapchainSemaphore, renderSemaphore;
	VkFence renderFence;
};

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

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkExtent2D swapchainExtent;

	FrameData frames[FRAME_OVERLAP];

	FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamily;

	void init();
	void cleanup();
	void draw();
	void run();

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();

	void createSwapchain(uint32_t const width, uint32_t const height);
	void destroySwapchain();
};
