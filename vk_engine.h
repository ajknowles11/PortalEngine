// Inspired heavily by vkguide, but manually implemented for my own understanding.

#pragma once

#include "camera.h"

#include "scene.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_types.h"

unsigned int constexpr FRAME_OVERLAP = 2;

struct CallbackQueue
{
	std::deque<std::function<void()>> functions;

	void pushFunction(std::function<void()>&& function) {
		functions.push_back(function);
	}

	void flush() {
		// reverse iterate the queue to execute all the functions
		for (auto it = functions.rbegin(); it != functions.rend(); ++it) {
			(*it)(); //call functors
		}

		functions.clear();
	}
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

struct RenderObject 
{
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;
	Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct FrameData
{
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	VkSemaphore swapchainSemaphore, renderSemaphore;
	VkFence renderFence;

	CallbackQueue deletionQueue;
	DescriptorAllocatorGrowable frameDescriptors;

	CallbackQueue postFrameQueue;
};

struct DrawContext
{
	std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
};

struct MeshNode : public Node
{
	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(glm::mat4 const& topMatrix, DrawContext& ctx) override;
};

struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants 
	{
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;

		glm::vec4 extra[14];
	};

	struct MaterialResources
	{
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void buildPipelines(VulkanEngine* engine);
	void clearResources(VkDevice device);

	MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, MaterialResources const& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct EngineStats
{
	float fps;
	float frameTime;
	int triangleCount;
	int drawCallCount;
	float sceneUpdateTime;
	float meshDrawTime;
};

class VulkanEngine
{
public:

	Camera mainCamera;
	Camera freeCamera;

	enum CameraMode : int
	{
		Default,    // Default
		Free,       // Control free cam, culling controlled by main cam
		Detached    // Control main cam but view from free cam
	} cameraMode = Default;

	bool vSyncEnabled = false;

	EngineStats stats;

	bool isInitialized{ false };
	int frameNumber{ 0 };
	bool stopRendering{ false };
	VkExtent2D windowExtent{ 1920, 1080 };
	std::string windowTitle = "Portal Engine";

	struct SDL_Window* window{ nullptr };

	std::string baseAppPath;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice selectedGPU;
	VkDevice device;
	VkSurfaceKHR surface;

	DescriptorAllocatorGrowable globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;

	VkPipelineLayout gradientPipelineLayout;

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;

	AllocatedImage drawImage;
	AllocatedImage depthImage;
	VkExtent2D drawExtent = { 1, 1 };
	float renderScale = 1.0f;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkExtent2D swapchainExtent;

	bool recreateSwapchainRequested = false;

	FrameData frames[FRAME_OVERLAP];

	FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamily;

	CallbackQueue mainDeletionQueue;
	CallbackQueue sceneDeletionQueue;

	VmaAllocator allocator;

	VkFence immFence;
	VkCommandBuffer immCommandBuffer;
	VkCommandPool immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect = 0;

	GPUSceneData sceneData;
	VkDescriptorSetLayout gpuSceneDataDescriptorLayout;

	AllocatedImage whiteImage;
	AllocatedImage blackImage;
	AllocatedImage grayImage;
	AllocatedImage errorCheckerboardImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;

	VkDescriptorSetLayout singleImageDescriptorLayout;

	MaterialInstance defaultData;
	std::shared_ptr<GLTFMaterial> defaultMaterial;
	GLTFMetallic_Roughness metalRoughMaterial;

	MaterialPipeline defaultPipeline;
	VkDescriptorSetLayout defaultDescriptorLayout;
	RenderObject cube;

	DrawContext mainDrawContext;

	Scene scene;
	bool indirectDrawInitialized = false;

	AllocatedBuffer drawIndirectCommandBuffer;

	void init();
	void cleanup();
	void queueLoadScene(std::string filePath);
	void loadScene(std::string_view filePath);
	void saveScene(std::shared_ptr<LoadedGLTF> scene) {}
	void draw();
	void drawBackground(VkCommandBuffer cmd) const;
	void drawGeometry(VkCommandBuffer cmd);
	void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) const;
	void updateScene(float delta);
	void run();

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) const;

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;
	void destroyBuffer(AllocatedBuffer const& buffer) const;

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;
	AllocatedImage createImage(void const* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false) const;
	void destroyImage(AllocatedImage const& img) const;

private:

	void initScene(std::shared_ptr<LoadedGLTF> newScene);
	void cleanupScene();

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();

	void initDescriptors();

	void initPipelines();
	void initBackgroundPipelines();
	void initDebugPipelines();
	//void initMeshPipeline();

	void initDefaultData();

	void initImgui();

	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain() const;
	void recreateSwapchain();
};
