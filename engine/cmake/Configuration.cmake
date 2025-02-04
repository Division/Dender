set(vk_engine_bin_dir ${CMAKE_CURRENT_LIST_DIR}/../../bin)

function(vk_engine_set_build_params)
	set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW PARENT_SCOPE)

	if (MSVC)
		cmake_policy(SET CMP0091 NEW)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" PARENT_SCOPE)
	endif()

	set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
	set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1 /D_DEBUG" PARENT_SCOPE)
	set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /Zi" PARENT_SCOPE)
	set(CMAKE_C_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1 /D_DEBUG" PARENT_SCOPE)
	set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG" PARENT_SCOPE)

	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" PARENT_SCOPE)
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" PARENT_SCOPE)
	set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" PARENT_SCOPE)

	set(GLFW_BUILD_DOCS false PARENT_SCOPE)
	set(GLFW_BUILD_EXAMPLES false PARENT_SCOPE)
	set(GLFW_BUILD_TESTS false PARENT_SCOPE)
	set(GLFW_INSTALL false PARENT_SCOPE)

	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${vk_engine_bin_dir} PARENT_SCOPE)
endfunction()