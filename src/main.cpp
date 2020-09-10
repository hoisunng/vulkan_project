#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <iostream>
#include "VulkanRenderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

GLFWwindow* window;
VulkanRenderer vulkanRenderer;

void initWindow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main()
{
	initWindow("Test Window", 800, 600);

	if (vulkanRenderer.init(window) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	auto angle = 0.0f;
	auto deltaTime = 0.0f;
	auto lastTime = 0.0f;

	auto spaceship = vulkanRenderer.createMeshModel("models/Intergalactic_Spaceship-(Wavefront).obj");

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		auto now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f) {
			angle -= 360.0f;
		}

		glm::mat4 testMat = glm::rotate(glm::mat4{ 1.0f }, glm::radians(angle), glm::vec3{ 0.0f, 1.0f, 0.0f });
		vulkanRenderer.updateModel(spaceship, testMat);

		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
