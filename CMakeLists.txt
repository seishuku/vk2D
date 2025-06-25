cmake_minimum_required (VERSION 3.8)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project ("vk2D" LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include("fetchDeps")
include("buildShaders")

fetchDeps()

set(PROJECT_SOURCES
	math/math.c
	math/matrix.c
	math/quat.c
	math/vec2.c
	math/vec3.c
	math/vec4.c
	system/memzone.c
	system/threads.c
	utils/base64.c
	utils/config.c
	utils/event.c
	utils/pipeline.c
	utils/spvparse.c
	utils/tokenizer.c
	vulkan/vulkan_buffer.c
	vulkan/vulkan_context.c
	vulkan/vulkan_descriptorset.c
	vulkan/vulkan_framebuffer.c
	vulkan/vulkan_heaps.c
	vulkan/vulkan_instance.c
	vulkan/vulkan_mem.c
	vulkan/vulkan_pipeline.c
	vulkan/vulkan_renderpass.c
	vulkan/vulkan_swapchain.c
	engine.c
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	add_definitions(-D_DEBUG)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	include("buildWindows")
	buildWindows()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	include("buildLinux")
	buildLinux()
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
	include("buildAndroid")
	buildAndroid()
endif()

buildShaders()