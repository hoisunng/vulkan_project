#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include "Utilities.h"
#include <glm/glm.hpp>

struct Model {
	glm::mat4 model;
};

class Mesh {
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices, std::vector<uint32_t>* indices);

	void setModel(glm::mat4 newModel);
	Model getModel() const;

	int getVertexCount() const;
	VkBuffer getVertexBuffer() const;
	int getIndexCount() const;
	VkBuffer getIndexBuffer() const;

	void destroyBuffers();

private:
	Model model;

	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	int indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,std::vector<Vertex>* vertices);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,std::vector<uint32_t>* indices);
};
