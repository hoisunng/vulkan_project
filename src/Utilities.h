#pragma once

#include <vulkan/vulkan_core.h>
#include <vector>
#include <fstream>

const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool isValid() const {
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentationModes;
};

struct SwapChainImage {
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file{filename, std::ios::binary | std::ios::ate};

	if (!file.is_open()) {
		throw std::runtime_error{ "Failed to open file!" };
	}

	const auto fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> fileBuffer( fileSize );

	file.seekg(0);

	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}