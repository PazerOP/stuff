cmake_minimum_required(VERSION 3.16.3)

include(CheckCXXCompilerFlag)

function(check_cxx_coroutine_support IS_SUPPORTED REQUIRED_FLAGS)
	set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

	check_cxx_compiler_flag(-fcoroutines COROUTINES_FLAG_FCOROUTINES)
	if (NOT COROUTINES_FLAG_FCOROUTINES)
		check_cxx_compiler_flag(-fcoroutines-ts COROUTINES_FLAG_FCOROUTINES_TS)
	endif()

	set(${REQUIRED_FLAGS})
	if (COROUTINES_FLAG_FCOROUTINES)
		set(${REQUIRED_FLAGS} -fcoroutines)
	elseif (COROUTINES_FLAG_FCOROUTINES_TS)
		set(${REQUIRED_FLAGS} -fcoroutines-ts)
	endif()

	get_directory_property(DIRECTORY_CXX_OPTS COMPILE_OPTIONS)
	get_directory_property(DIRECTORY_LINK_OPTS LINK_OPTIONS)

	try_compile(${IS_SUPPORTED}
		${CMAKE_CURRENT_BINARY_DIR}
		"${PROJECT_SOURCE_DIR}/cmake/CheckCoroutineSupport.cpp"
		CXX_STANDARD 20
		COMPILE_DEFINITIONS "${${REQUIRED_FLAGS}} ${DIRECTORY_CXX_OPTS}"
		LINK_OPTIONS ${DIRECTORY_LINK_OPTS}
		OUTPUT_VARIABLE TRY_COMPILE_OUTPUT)

	message("check_cxx_coroutine_support ${IS_SUPPORTED} = ${${IS_SUPPORTED}}, ${REQUIRED_FLAGS} = ${${REQUIRED_FLAGS}}")
	if (NOT ${${IS_SUPPORTED}})
		message("check_cxx_coroutine_support output = ${TRY_COMPILE_OUTPUT}")
	endif()

endfunction()
