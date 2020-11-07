# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project("VulkanProject")

find_package(Vulkan REQUIRED)

include(cmake/conan_setup.cmake)

# Include sub-projects.
add_subdirectory("src")
add_subdirectory("shaders")
