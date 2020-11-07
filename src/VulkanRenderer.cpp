#include "VulkanRenderer.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}


int VulkanRenderer::init(GLFWwindow* newWindow)
{
	window = newWindow;

	try
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createColourBufferImage();
		createDepthBufferImage();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createTextureSampler();
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createInputDescriptorSets();
		createSynchronisation();

		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, 0.1f, 100.0f);
		uboViewProjection.projection[1][1] *= -1;
		uboViewProjection.view = glm::lookAt(glm::vec3{ 10.0f, 0.0f, 15.0f }, glm::vec3{ 0.0f,0.0f,0.0f }, glm::vec3{ 0.0f,1.0f,0.0f });

		createTexture("plain.png");
	}
	catch (const std::runtime_error& e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	return 0;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
	if (modelId >= modelList.size()) {
		return;
	}

	modelList[modelId].setModel(newModel);
}

void VulkanRenderer::draw()
{
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	recordCommands(imageIndex);

	updateUniformBuffers(imageIndex);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];
	auto result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to submit Command Buffer to Queue!" };
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to present Image!" };
	}

	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	for (auto i = 0u; i < modelList.size(); ++i) {
		modelList[i].destroyMeshModel();
	}

	for (auto i = 0u; i < textureImages.size(); ++i) {
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}

	for (auto i = 0u; i < colourBufferImage.size(); ++i) {
		vkDestroyImageView(mainDevice.logicalDevice, colourBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, colourBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, colourBufferImageMemory[i], nullptr);
	}
	for (auto i = 0u; i < depthBufferImage.size(); ++i) {
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);
	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
	for (auto i = 0u; i < swapChainImages.size(); ++i) {
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
		//vkDestroyBuffer(mainDevice.logicalDevice, modelDUniformBuffer[i], nullptr);
		//vkFreeMemory(mainDevice.logicalDevice, modelDUniformBufferMemory[i], nullptr);
	}
	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);
	for (auto i = 0; i < MAX_FRAME_DRAWS; ++i) {
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (const auto& framebuffer : swapChainFramebuffers) {
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (const auto& image : swapChainImages) {
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapChain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	if (enableValidationLayers) {
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "None";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	uint32_t glfwExtensionCount = 0;
	const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> instanceExtensions{ glfwExtensions, glfwExtensions + glfwExtensionCount };
	if (enableValidationLayers) {
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	if (!checkInstanceExtensionSupport(instanceExtensions)) {
		throw std::runtime_error{ "VkInstance does not support required extensions!" };
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error{ "VkInstance does not support required layers!" };
	}

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.pNext = nullptr;
	}

	const auto result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create Vulkan Instance!" };
	}
}

void VulkanRenderer::setupDebugMessenger()
{
	if (!enableValidationLayers) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to set up debug messenger!" };
	}
}

void VulkanRenderer::createLogicalDevice()
{
	const auto indices = getQueueFamilies(mainDevice.physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices{ indices.graphicsFamily, indices.presentationFamily };
	for (const auto& queueFamilyIndex : queueFamilyIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		auto priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	const auto result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Logical Device!" };
	}

	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface()
{
	const auto result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Surface!" };
	}
}

void VulkanRenderer::createSwapChain()
{
	const auto swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

	const auto surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	const auto presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	const auto extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount) {
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;

	const auto indices = getQueueFamilies(mainDevice.physicalDevice);
	if (indices.graphicsFamily != indices.presentationFamily) {
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentationFamily };
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	const auto result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapChain);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a swap chain!" };
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images{ swapChainImageCount };
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapChain, &swapChainImageCount, images.data());

	for (const auto& image : images) {
		SwapChainImage swapChainImage = {};
		swapChainImage.image = image;
		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::createRenderPass()
{
	std::array<VkSubpassDescription, 2> subpasses = {};

	// SUBPASS 1
	VkAttachmentDescription colourAttachment = {};
	colourAttachment.format = chooseSupportedFormat({ VK_FORMAT_R8G8B8A8_UNORM }, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = chooseSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colourAttachmentReference = {};
	colourAttachmentReference.attachment = 1;
	colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 2;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colourAttachmentReference;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;

	// SUBPASS 2
	VkAttachmentDescription swapchainColourAttachment = {};
	swapchainColourAttachment.format = swapChainImageFormat;
	swapchainColourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapchainColourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainColourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapchainColourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainColourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapchainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference swapchainColourAttachmentReference = {};
	swapchainColourAttachmentReference.attachment = 0;
	swapchainColourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 2> inputReferences;
	inputReferences[0].attachment = 1;
	inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[1].attachment = 2;
	inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].colorAttachmentCount = 1;
	subpasses[1].pColorAttachments = &swapchainColourAttachmentReference;
	subpasses[1].pDepthStencilAttachment = nullptr;
	subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputReferences.size());
	subpasses[1].pInputAttachments = inputReferences.data();

	std::array<VkSubpassDependency, 3> subpassDependencies;
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[1].dstSubpass = 1;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	subpassDependencies[2].srcSubpass = 1;
	subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[2].dependencyFlags = 0;

	std::array<VkAttachmentDescription, 3> renderPassAttachments = { swapchainColourAttachment, colourAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses = subpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	const auto result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Render Pass!" };
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	vpLayoutBinding.binding = 0;
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpLayoutBinding.descriptorCount = 1;
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vpLayoutBinding.pImmutableSamplers = nullptr;

	//VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	//modelLayoutBinding.binding = 1;
	//modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	//modelLayoutBinding.descriptorCount = 1;
	//modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	//modelLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutCreateInfo.pBindings = layoutBindings.data();

	auto result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Descriptor Set Layout!" };
	}

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Sampler Set Layout!" };
	}

	VkDescriptorSetLayoutBinding colourLayoutBinding = {};
	colourLayoutBinding.binding = 0;
	colourLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colourLayoutBinding.descriptorCount = 1;
	colourLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	colourLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding depthLayoutBinding = {};
	depthLayoutBinding.binding = 1;
	depthLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthLayoutBinding.descriptorCount = 1;
	depthLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	depthLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> inputBindings = { colourLayoutBinding, depthLayoutBinding };

	VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo = {};
	inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
	inputLayoutCreateInfo.pBindings = inputBindings.data();

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputSetLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Input Set Layout!" };
	}
}

void VulkanRenderer::createPushConstantRange()
{
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Model);
}

void VulkanRenderer::createGraphicsPipeline()
{
	const auto vertexShaderCode = readFile("shaders/shader.vert.spv");
	const auto fragmentShaderCode = readFile("shaders/shader.frag.spv");

	const auto vertexShaderModule = createShaderModule(vertexShaderCode);
	const auto fragmentShaderModule = createShaderModule(fragmentShaderCode);

	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, pos);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, col);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, tex);

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0,0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.0f;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colourState = {};
	colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colourState.blendEnable = VK_TRUE;
	colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colourState.colorBlendOp = VK_BLEND_OP_ADD;
	colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colourState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
	colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlendingCreateInfo.logicOpEnable = VK_FALSE;
	colourBlendingCreateInfo.attachmentCount = 1;
	colourBlendingCreateInfo.pAttachments = &colourState;

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { descriptorSetLayout, samplerSetLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	auto result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create Pipeline Layout!" };
	}

	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Pipeline!" };
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

	const auto secondVertexShaderCode = readFile("shaders/second.vert.spv");
	const auto secondFragmentShaderCode = readFile("shaders/second.frag.spv");

	const auto secondVertexShaderModule = createShaderModule(secondVertexShaderCode);
	const auto secondFragmentShaderModule = createShaderModule(secondFragmentShaderCode);

	vertexShaderCreateInfo.module = secondVertexShaderModule;
	fragmentShaderCreateInfo.module = secondFragmentShaderModule;

	VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;

	VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo = {};
	secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	secondPipelineLayoutCreateInfo.setLayoutCount = 1;
	secondPipelineLayoutCreateInfo.pSetLayouts = &inputSetLayout;
	secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create second Pipeline Layout!" };
	}

	pipelineCreateInfo.pStages = secondShaderStages;
	pipelineCreateInfo.layout = secondPipelineLayout;
	pipelineCreateInfo.subpass = 1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create second Pipeline!" };
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, secondFragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, secondVertexShaderModule, nullptr);
}

void VulkanRenderer::createColourBufferImage()
{
	colourBufferImage.resize(swapChainImages.size());
	colourBufferImageMemory.resize(swapChainImages.size());
	colourBufferImageView.resize(swapChainImages.size());

	const auto colourFormat = chooseSupportedFormat({ VK_FORMAT_R8G8B8A8_UNORM }, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	for (auto i = 0u; i < swapChainImages.size(); ++i) {
		colourBufferImage[i] = createImage(swapChainExtent.width, swapChainExtent.height, colourFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colourBufferImageMemory[i]);
		colourBufferImageView[i] = createImageView(colourBufferImage[i], colourFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VulkanRenderer::createDepthBufferImage()
{
	depthBufferImage.resize(swapChainImages.size());
	depthBufferImageMemory.resize(swapChainImages.size());
	depthBufferImageView.resize(swapChainImages.size());

	const auto depthFormat = chooseSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	for (auto i = 0u; i < swapChainImages.size(); ++i) {
		depthBufferImage[i] = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory[i]);
		depthBufferImageView[i] = createImageView(depthBufferImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
}

void VulkanRenderer::createFramebuffers()
{
	swapChainFramebuffers.resize(swapChainImages.size());
	for (auto i = 0u; i < swapChainFramebuffers.size(); ++i) {
		std::array<VkImageView, 3 > attachments = {
			swapChainImages[i].imageView,
			colourBufferImageView[i],
			depthBufferImageView[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = swapChainExtent.width;
		framebufferCreateInfo.height = swapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		const auto result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error{ "Failed to create a Framebuffer!" };
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	const auto queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

	const auto result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Command Pool!" };
	}
}

void VulkanRenderer::createCommandBuffers()
{
	commandBuffers.resize(swapChainImages.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	const auto result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to allocate Command Buffers!" };
	}
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].color = { 0.6f, 0.65f, 0.4f, 1.0f };
	clearValues[2].depthStencil.depth = 1.0f;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];

	auto result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to start recording a Command Buffer!" };
	}

	vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	for (auto j = 0u; j < modelList.size(); ++j) {
		const auto& thisModel = modelList[j];

		vkCmdPushConstants(commandBuffers[currentImage], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model), &thisModel.getModel());

		for (auto k = 0u; k < thisModel.getMeshCount(); ++k) {
			VkBuffer vertexBuffers[] = { thisModel.getMesh(k)->getVertexBuffer() };

			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);


			//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment * j);
			std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[currentImage], samplerDescriptorSets[thisModel.getMesh(k)->getTexId()] };
			vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

			vkCmdDrawIndexed(commandBuffers[currentImage], static_cast<uint32_t>(thisModel.getMesh(k)->getIndexCount()), 1, 0, 0, 0);
		}
	}

	vkCmdNextSubpass(commandBuffers[currentImage], VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
	vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout, 0, 1, &inputDescriptorSets[currentImage], 0, nullptr);
	vkCmdDraw(commandBuffers[currentImage], 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffers[currentImage]);

	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to stop recording a Command Buffer!" };
	}
}

void VulkanRenderer::createSynchronisation()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (auto i = 0; i < MAX_FRAME_DRAWS; ++i) {
		if ((vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS) ||
			(vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS) ||
			(vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)) {
			throw std::runtime_error{ "Failed to create Semaphores and/or Fence!" };
		}
	}
}

void VulkanRenderer::createTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.maxAnisotropy = 16;

	const auto result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Texture Sampler!" };
	}
}

void VulkanRenderer::createUniformBuffers()
{
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);
	//VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;

	vpUniformBuffer.resize(swapChainImages.size());
	vpUniformBufferMemory.resize(swapChainImages.size());
	//modelDUniformBuffer.resize(swapChainImagessize());
	//modelDUniformBufferMemory.resize(swapChainImages.size());

	for (auto i = 0u; i < swapChainImages.size(); ++i) {
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMemory[i]);
		//createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDUniformBuffer[i], &modelDUniformBufferMemory[i]);
	}
}

void VulkanRenderer::createDescriptorPool()
{
	VkDescriptorPoolSize vpPoolSize;
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	//VkDescriptorPoolSize modelPoolSize;
	//modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	//modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDUniformBuffer.size());

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { vpPoolSize };

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	auto result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Descriptor Pool!" };
	}

	VkDescriptorPoolSize samplerPoolSize;
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS;

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;
	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Sampler Descriptor Pool!" };
	}

	VkDescriptorPoolSize colourInputPoolSize = {};
	colourInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colourInputPoolSize.descriptorCount = static_cast<uint32_t>(colourBufferImageView.size());

	VkDescriptorPoolSize depthInputPoolSize = {};
	depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

	std::vector<VkDescriptorPoolSize> inputPoolSizes = { colourInputPoolSize, depthInputPoolSize };

	VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
	inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	inputPoolCreateInfo.maxSets = swapChainImages.size();
	inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
	inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();
	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a Sampler Descriptor Pool!" };
	}
}

void VulkanRenderer::createDescriptorSets()
{
	descriptorSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	const auto result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to allocate Descriptor Sets!" };
	}

	for (auto i = 0u; i < swapChainImages.size(); ++i) {
		VkDescriptorBufferInfo vpBufferInfo;
		vpBufferInfo.buffer = vpUniformBuffer[i];
		vpBufferInfo.offset = 0;
		vpBufferInfo.range = sizeof(UboViewProjection);

		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];
		vpSetWrite.dstBinding = 0;
		vpSetWrite.dstArrayElement = 0;
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpSetWrite.descriptorCount = 1;
		vpSetWrite.pBufferInfo = &vpBufferInfo;

		//VkDescriptorBufferInfo modelBufferInfo;
		//modelBufferInfo.buffer = modelDUniformBuffer[i];
		//modelBufferInfo.offset = 0;
		//modelBufferInfo.range = modelUniformAlignment;

		//VkWriteDescriptorSet modelSetWrite = {};
		//modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		//modelSetWrite.dstSet = descriptorSets[i];
		//modelSetWrite.dstBinding = 1;
		//modelSetWrite.dstArrayElement = 0;
		//modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		//modelSetWrite.descriptorCount = 1;
		//modelSetWrite.pBufferInfo = &modelBufferInfo;

		std::vector<VkWriteDescriptorSet> setWrites = { vpSetWrite };
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::createInputDescriptorSets()
{
	inputDescriptorSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), inputSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = inputDescriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	const auto result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to allocate Input Descriptor Sets!" };
	}

	for (auto i = 0u; i < swapChainImages.size(); ++i) {
		VkDescriptorImageInfo colourAttachmentDescriptor = {};
		colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		colourAttachmentDescriptor.imageView = colourBufferImageView[i];
		colourAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet colourWrite = {};
		colourWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colourWrite.dstSet = inputDescriptorSets[i];
		colourWrite.dstBinding = 0;
		colourWrite.dstArrayElement = 0;
		colourWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		colourWrite.descriptorCount = 1;
		colourWrite.pImageInfo = &colourAttachmentDescriptor;

		VkDescriptorImageInfo depthAttachmentDescriptor = {};
		depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthAttachmentDescriptor.imageView = depthBufferImageView[i];
		depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet depthWrite = {};
		depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depthWrite.dstSet = inputDescriptorSets[i];
		depthWrite.dstBinding = 1;
		depthWrite.dstArrayElement = 0;
		depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthWrite.descriptorCount = 1;
		depthWrite.pImageInfo = &depthAttachmentDescriptor;

		std::vector<VkWriteDescriptorSet> setWrites = { colourWrite, depthWrite };
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	void* data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	std::memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	//for (auto i = 0u; i < meshList.size(); ++i) {
	//	auto thisModel = (UboModel*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
	//	*thisModel = meshList[i].getModel();
	//}

	//vkMapMemory(mainDevice.logicalDevice, modelDUniformBufferMemory[imageIndex], 0, modelUniformAlignment * meshList.size(), 0, &data);
	//std::memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
	//vkUnmapMemory(mainDevice.logicalDevice, modelDUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::getPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error{ "Can't find GPUs that support Vulkan instance!" };
	}

	std::vector<VkPhysicalDevice> deviceList{ deviceCount };
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList) {
		if (checkDeviceSuitable(device)) {
			mainDevice.physicalDevice = device;
			break;
		}
	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

//void VulkanRenderer::allocateDynamicBufferTransferSpace()
//{
//	modelUniformAlignment = (sizeof(UboModel) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);
//
//	modelTransferSpace = (UboModel*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
//}

bool VulkanRenderer::checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions{ extensionCount };
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (const auto& checkExtension : checkExtensions) {
		bool hasExtension = false;
		for (const auto& extension : extensions) {
			if (strcmp(checkExtension, extension.extensionName) == 0) {
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0) {
		return false;
	}

	std::vector<VkExtensionProperties> extensions{ extensionCount };
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	for (const auto& deviceExtension : deviceExtensions) {
		bool hasExtension = false;
		for (const auto& extension : extensions) {
			if (strcmp(deviceExtension, extension.extensionName) == 0) {
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers{ layerCount };
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	/*
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	*/

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	const auto indices = getQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainValid = false;
	if (extensionsSupported) {
		const auto swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.isValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList{ queueFamilyCount };
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	auto i = 0;
	for (const auto& queueFamily : queueFamilyList) {
		if ((queueFamily.queueCount > 0) && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		if ((queueFamily.queueCount > 0) && presentationSupport) {
			indices.presentationFamily = i;
		}

		if (indices.isValid()) {
			break;
		}

		++i;
	}
	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount > 0) {
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	uint32_t presentationCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	if (presentationCount > 0) {
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : formats) {
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const auto& presentationMode : presentationModes) {
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentationMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return surfaceCapabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	for (const auto& format : formats) {
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags) {
			return format;
		}
	}

	throw std::runtime_error{ "Failed to find a matching format!" };
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = useFlags;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkImage image;
	auto result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create an Image!" };
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to allocate memory for image!" };
	}

	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	const auto result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create an image view!" };
	}
	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	const auto result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to create a shader module!" };
	}
	return shaderModule;
}

int VulkanRenderer::createTextureImage(const std::string& fileName)
{
	int width, height;
	VkDeviceSize imageSize;

	const auto imageData = loadTextureFile(fileName, &width, &height, &imageSize);

	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer, &imageStagingBufferMemory);

	void* data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	std::memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	stbi_image_free(imageData);

	VkDeviceMemory texImageMemory;
	const auto texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(const std::string& fileName)
{
	const auto textureImageLoc = createTextureImage(fileName);

	const auto imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	const auto descriptorLoc = createTextureDescriptor(imageView);

	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSet descriptorSet;

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerSetLayout;

	const auto result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	if (result != VK_SUCCESS) {
		throw std::runtime_error{ "Failed to allocate Texture Descriptor Sets!" };
	}

	VkDescriptorImageInfo imageInfo;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = textureImage;
	imageInfo.sampler = textureSampler;

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	samplerDescriptorSets.push_back(descriptorSet);

	return samplerDescriptorSets.size() - 1;
}

int VulkanRenderer::createMeshModel(const std::string& modelFile)
{
	Assimp::Importer importer;
	const auto scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	if (!scene) {
		throw std::runtime_error{ "Failed to load Model! (" + modelFile + ")" };
	}

	const auto textureNames = MeshModel::LoadMaterials(scene);
	std::vector<int> matToTex(textureNames.size());

	for (auto i = 0u; i < textureNames.size(); ++i) {
		if (textureNames[i].empty()) {
			matToTex[i] = 0;
		}
		else {
			matToTex[i] = createTexture(textureNames[i]);
		}
	}

	auto modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, scene->mRootNode, scene, matToTex);

	auto meshModel = MeshModel(std::move(modelMeshes));
	modelList.push_back(meshModel);
	return modelList.size() - 1;
}

stbi_uc* VulkanRenderer::loadTextureFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize)
{
	int channels;

	const auto fileLoc = "textures/" + fileName;
	const auto image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image) {
		throw std::runtime_error{ "Failed to load a texture file! (" + fileName + ")" };
	}

	*imageSize = *width * *height * 4;
	return image;
}

VkBool32 VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << "\n";

	return VK_FALSE;
}
