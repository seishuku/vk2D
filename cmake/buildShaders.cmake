function("buildShaders")
	set(SHADER_SOURCES
	)

	find_package(Vulkan COMPONENTS glslc)
	find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

	foreach(SOURCE ${SHADER_SOURCES})
		set(SHADER_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}.spv)
		set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE})
		add_custom_command(
			OUTPUT ${SHADER_OUTPUT}
			DEPENDS ${SOURCE}
			COMMAND ${GLSLC} --target-env=vulkan1.1 -O -o ${SHADER_OUTPUT} ${SHADER_SOURCE}
		)
		list(APPEND SHADER_BINARIES ${SHADER_OUTPUT})
	endforeach()

	add_custom_target(ShaderCompilation DEPENDS ${SHADER_BINARIES})
endFunction()