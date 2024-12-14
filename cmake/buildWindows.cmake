function("buildWindows")
	add_definitions(-DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CONSOLE)
	list(APPEND PROJECT_SOURCES system/win32.c)

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
    )

    if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC /experimental:c11atomics)
    endif()
endFunction()
