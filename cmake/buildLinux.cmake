function("buildLinux")
	if(WAYLAND)
		add_definitions(-DLINUX -DWAYLAND -g)
		list(APPEND PROJECT_SOURCES system/linux_wayland.c system/xdg-shell.c system/relative-pointer.c)
	else()
		add_definitions(-DLINUX -g)
		list(APPEND PROJECT_SOURCES system/linux_x11.c)
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
	#add_dependencies(${CMAKE_PROJECT_NAME} ShaderCompilation)

	target_link_libraries(
		${CMAKE_PROJECT_NAME} PUBLIC
		Vulkan::Vulkan
		m
	)

	if(WAYLAND)
		target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC xkbcommon wayland-client)
	else()
		target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC X11 Xi Xfixes)
	endif()
endFunction()