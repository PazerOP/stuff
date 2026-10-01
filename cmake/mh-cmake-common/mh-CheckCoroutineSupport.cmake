cmake_minimum_required(VERSION 3.17)

include(CheckCXXSourceCompiles)

function(mh_check_cxx_coroutine_support IS_SUPPORTED_OUT REQUIRED_FLAGS_OUT)
	# Modern approach: check if C++20 is supported first
	if(NOT "cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
		set(${IS_SUPPORTED_OUT} FALSE PARENT_SCOPE)
		set(${REQUIRED_FLAGS_OUT} "" PARENT_SCOPE)
		message("${CMAKE_CURRENT_FUNCTION}(${IS_SUPPORTED_OUT}=FALSE ${REQUIRED_FLAGS_OUT}=) - C++20 not supported")
		return()
	endif()

	# Test actual coroutines functionality with C++20.
	# check_cxx_source_compiles uses the compiler's DEFAULT standard unless told
	# otherwise; <coroutine> defines nothing pre-C++20, so without this every
	# compiler reported "unsupported" (including the GCC 10 -fcoroutines case
	# this machinery exists to detect).
	if (MSVC)
		set(CMAKE_REQUIRED_FLAGS "/std:c++20")
	else()
		set(CMAKE_REQUIRED_FLAGS "-std=c++20")
	endif()
	set(CMAKE_REQUIRED_QUIET TRUE)
	check_cxx_source_compiles("
#include <coroutine>
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};
Task test_coroutine() { co_return; }
int main() { return 0; }
" COROUTINES_WORK_WITHOUT_FLAGS)

	set(REQUIRED_FLAGS "")
	set(IS_SUPPORTED ${COROUTINES_WORK_WITHOUT_FLAGS})

	# If coroutines don't work without flags, try with -fcoroutines (GCC 10)
	if(NOT COROUTINES_WORK_WITHOUT_FLAGS AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_REQUIRED_FLAGS "-std=c++20 -fcoroutines")
		check_cxx_source_compiles("
#include <coroutine>
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};
Task test_coroutine() { co_return; }
int main() { return 0; }
" COROUTINES_WORK_WITH_FCOROUTINES)
		
		if(COROUTINES_WORK_WITH_FCOROUTINES)
			set(REQUIRED_FLAGS "-fcoroutines")
			set(IS_SUPPORTED TRUE)
		endif()
		set(CMAKE_REQUIRED_FLAGS "")
	endif()

	message("${CMAKE_CURRENT_FUNCTION}(${IS_SUPPORTED_OUT}=${IS_SUPPORTED} ${REQUIRED_FLAGS_OUT}=${REQUIRED_FLAGS})")

	set(${IS_SUPPORTED_OUT} ${IS_SUPPORTED} PARENT_SCOPE)
	set(${REQUIRED_FLAGS_OUT} ${REQUIRED_FLAGS} PARENT_SCOPE)

endfunction()
