#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

using namespace std;

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	const auto window = glfwCreateWindow(800, 600, "Test Window", nullptr, nullptr);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::cout << "Extension count: " << extensionCount << "\n";

	glm::mat4 testMatrix{ 1.0 };
	glm::vec4 testVector{ 1.0 };
	auto testResult = testMatrix * testVector;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	glfwTerminate();

	return 0;
}
