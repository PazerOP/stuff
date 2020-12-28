cmake_minimum_required(VERSION 3.16.3)

include(CheckCXXCompilerFlag)

function(check_cxx_coroutine_support IS_SUPPORTED COROUTINES_FLAGS)
	set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

	check_cxx_compiler_flag(-fcoroutines COROUTINES_FLAG_FCOROUTINES)
	if (NOT COROUTINES_FLAG_FCOROUTINES)
		check_cxx_compiler_flag(-fcoroutines-ts COROUTINES_FLAG_FCOROUTINES_TS)
	endif()

	if (COROUTINES_FLAG_FCOROUTINES)
		set(COROUTINES_FLAGS -fcoroutines)
	elseif (COROUTINES_FLAG_FCOROUTINES_TS)
		set(COROUTINES_FLAGS -fcoroutines-ts)
	endif()

	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${COROUTINES_FLAG})
	set(CMAKE_CXX_STANDARD 20)

	check_cxx_source_compiles(
	"#include <coroutine> \
	int main(int argc, char** argv) { std::coroutine_handle<> handle; return 0; }"
		${IS_SUPPORTED})

	# try_compile(${PROJECT_NAME}_SUPPORTS_COROUTINES ${CMAKE_CURRENT_BINARY_DIR} "${CMAKE_CURRENT_LIST_DIR}/CheckCoroutineSupport.cpp"
	# 	CXX_STANDARD 20)

	message("check_cxx_coroutine_support IS_SUPPORTED = ${${IS_SUPPORTED}}")
endfunction()

