cmake_minimum_required(VERSION 3.16.3)

include(CheckCXXCompilerFlag)

function(check_cxx_coroutine_support IS_SUPPORTED_OUT REQUIRED_FLAGS_OUT)
	set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

	check_cxx_compiler_flag(-fcoroutines COROUTINES_FLAG_FCOROUTINES)
	if (NOT COROUTINES_FLAG_FCOROUTINES)
		check_cxx_compiler_flag(-fcoroutines-ts COROUTINES_FLAG_FCOROUTINES_TS)
	endif()

	set(REQUIRED_FLAGS "")
	if (COROUTINES_FLAG_FCOROUTINES)
		set(REQUIRED_FLAGS "-fcoroutines")
	elseif (COROUTINES_FLAG_FCOROUTINES_TS)
		set(REQUIRED_FLAGS "-fcoroutines-ts")
	endif()

	get_directory_property(DIRECTORY_CXX_OPTS COMPILE_OPTIONS)
	get_directory_property(DIRECTORY_LINK_OPTS LINK_OPTIONS)
	message("DIRECTORY_CXX_OPTS = ${DIRECTORY_CXX_OPTS}, DIRECTORY_LINK_OPTS = ${DIRECTORY_LINK_OPTS}")

	try_compile(IS_SUPPORTED
		${CMAKE_CURRENT_BINARY_DIR}
		"${PROJECT_SOURCE_DIR}/cmake/CheckCoroutineSupport.cpp"
		CXX_STANDARD 20
		COMPILE_DEFINITIONS "${REQUIRED_FLAGS} ${DIRECTORY_CXX_OPTS}"
		LINK_OPTIONS ${DIRECTORY_LINK_OPTS}
		OUTPUT_VARIABLE TRY_COMPILE_OUTPUT
	)

	message("check_cxx_coroutine_support(${IS_SUPPORTED_OUT}=${IS_SUPPORTED} ${REQUIRED_FLAGS_OUT}=${REQUIRED_FLAGS})")
	if (NOT IS_SUPPORTED)
		message("check_cxx_coroutine_support output = ${TRY_COMPILE_OUTPUT}")
	endif()

	set(${IS_SUPPORTED_OUT} ${IS_SUPPORTED} PARENT_SCOPE)
	set(${REQUIRED_FLAGS_OUT} ${REQUIRED_FLAGS} PARENT_SCOPE)

endfunction()
