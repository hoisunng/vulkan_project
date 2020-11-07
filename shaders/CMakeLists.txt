cmake_minimum_required (VERSION 3.8)

find_program(GLSL_VALIDATOR glslangValidator REQUIRED)
if(NOT GLSL_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found!")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "*.frag"
    "*.vert"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
