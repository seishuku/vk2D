include(FetchContent)

function(fetchDeps)
	find_package(Python COMPONENTS Interpreter)

	find_package(Vulkan COMPONENTS glslc)
	find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)
endFunction()
