# CMakeList.txt : CMake project for VulkanProject, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (VulkanProject 
	"main.cpp" 
	"VulkanRenderer.cpp" "Utilities.h")

target_link_libraries(VulkanProject PRIVATE 
	Vulkan::Vulkan
	CONAN_PKG::glfw
	CONAN_PKG::glm)

# TODO: Add tests and install targets if needed.
