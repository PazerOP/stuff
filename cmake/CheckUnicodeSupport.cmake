cmake_minimum_required(VERSION 3.16.3)

include(CheckCXXSourceCompiles)

function(check_cxx_unicode_support IS_SUPPORTED)

	get_directory_property(DIRECTORY_CXX_OPTS COMPILE_OPTIONS)
	get_directory_property(DIRECTORY_LINK_OPTS LINK_OPTIONS)

	try_compile(${IS_SUPPORTED}
		${CMAKE_CURRENT_BINARY_DIR}
		"${PROJECT_SOURCE_DIR}/cmake/CheckUnicodeSupport.cpp"
		CXX_STANDARD 20
		COMPILE_DEFINITIONS ${DIRECTORY_CXX_OPTS}
		LINK_OPTIONS ${DIRECTORY_LINK_OPTS}
		OUTPUT_VARIABLE TRY_COMPILE_OUTPUT)

	message("check_cxx_unicode_support IS_SUPPORTED = ${${IS_SUPPORTED}}")
	if (NOT ${${IS_SUPPORTED}})
		message("check_cxx_unicode_support output = ${TRY_COMPILE_OUTPUT}")
	endif()

endfunction()
