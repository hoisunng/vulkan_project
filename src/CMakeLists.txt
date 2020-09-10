# CMakeList.txt : CMake project for VulkanProject, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (VulkanProject 
	"main.cpp" 
	"Mesh.cpp"
	"MeshModel.cpp"
	"VulkanRenderer.cpp")

add_dependencies(VulkanProject Shaders)

target_link_libraries(VulkanProject PRIVATE 
	Vulkan::Vulkan
	CONAN_PKG::glfw
	CONAN_PKG::glm
	CONAN_PKG::stb
	CONAN_PKG::assimp)

add_custom_command(TARGET VulkanProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VulkanProject>/shaders/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:VulkanProject>/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VulkanProject>/textures/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/textures" "$<TARGET_FILE_DIR:VulkanProject>/textures"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VulkanProject>/models/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/models" "$<TARGET_FILE_DIR:VulkanProject>/models"
	)
