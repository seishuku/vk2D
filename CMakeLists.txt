cmake_minimum_required (VERSION 3.8)

include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project ("vk2D" LANGUAGES C)

find_package(Vulkan COMPONENTS glslc)

set(PROJECT_SOURCES
	math/math.c
	math/matrix.c
	math/quat.c
	math/vec2.c
	math/vec3.c
	math/vec4.c
	system/memzone.c
	system/threads.c
	utils/event.c
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
	add_definitions(-DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CONSOLE)
	list(APPEND PROJECT_SOURCES system/win32.c)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
if(WAYLAND)
	add_definitions(-DLINUX -DWAYLAND -g)
	list(APPEND PROJECT_SOURCES system/linux_wayland.c system/xdg-shell.c system/relative-pointer.c)
else()
	add_definitions(-DLINUX -g)
	list(APPEND PROJECT_SOURCES system/linux_x11.c)
endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        add_compile_options(
			"-march=x86-64-v3"
			"-ggdb3"
			"-fprofile-instr-generate"
			"/PROFILE"
		)
    else()
        message(WARNING "Unknown CPU architecture ${CMAKE_SYSTEM_PROCESSOR} not targeted.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        add_compile_options("/arch:AVX2")
        else()
            message(WARNING "Unknown CPU architecture ${CMAKE_SYSTEM_PROCESSOR} not targeted.")
    endif()
endif()

add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(
    ${CMAKE_PROJECT_NAME} PUBLIC
	Vulkan::Vulkan
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC xinput.lib)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
if(WAYLAND)
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC m xkbcommon wayland-client)
else()
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC m X11 Xi Xfixes)
endif()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC /experimental:c11atomics)
endif()
