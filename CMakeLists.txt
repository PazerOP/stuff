cmake_minimum_required(VERSION 3.17)

project(mh_stuff CXX)

option(MH_STUFF_COMPILE_LIBRARY "True to compile some code into to a static library for less overhead" ON)

set(MH_STUFF_SOURCES
	"cpp/include/mh/algorithm/algorithm.hpp"
	"cpp/include/mh/algorithm/multi_compare.hpp"

	"cpp/include/mh/concurrency/async.hpp"
	"cpp/include/mh/concurrency/main_thread.hpp"
	"cpp/include/mh/concurrency/thread_pool.hpp"

	"cpp/include/mh/data/bit_float.hpp"
	"cpp/include/mh/data/bits.hpp"
	"cpp/include/mh/data/variable_pusher.hpp"

	"cpp/include/mh/error/ensure.inl"
	"cpp/include/mh/error/ensure.hpp"
	"cpp/include/mh/error/error_code_exception.hpp"
	"cpp/include/mh/error/exception_details.hpp"
	"cpp/include/mh/error/exception_details.inl"
	"cpp/include/mh/error/expected.hpp"
	"cpp/include/mh/error/nested_exception.hpp"
	"cpp/include/mh/error/status.hpp"

	"cpp/include/mh/io/file.hpp"
	"cpp/include/mh/io/getopt.hpp"

	"cpp/include/mh/math/interpolation.hpp"
	"cpp/include/mh/math/uint128.hpp"

	"cpp/include/mh/memory/buffer.inl"
	"cpp/include/mh/memory/buffer.hpp"
	"cpp/include/mh/memory/cached_variable.hpp"
	"cpp/include/mh/memory/checked_ptr.hpp"
	"cpp/include/mh/memory/unique_object.hpp"

	"cpp/include/mh/raii/scope_exit.hpp"

	"cpp/include/mh/reflection/enum.hpp"

	"cpp/include/mh/text/formatters/error_code.hpp"
	"cpp/include/mh/text/case_insensitive_string.hpp"
	"cpp/include/mh/text/charconv_helper.hpp"
	"cpp/include/mh/text/codecvt.hpp"
	"cpp/include/mh/text/fmtstr.hpp"
	"cpp/include/mh/text/format.hpp"
	"cpp/include/mh/text/insertion_conversion.hpp"
	"cpp/include/mh/text/memstream.hpp"
	"cpp/include/mh/text/string_insertion.hpp"
	"cpp/include/mh/text/stringops.hpp"

	"cpp/include/mh/types/disable_copy_move.hpp"

	"cpp/include/mh/compiler.hpp"
	"cpp/include/mh/future.hpp"
	"cpp/include/mh/source_location.hpp"
	"cpp/include/mh/utility.hpp"
	"cpp/include/mh/variant.hpp"
)

include("cmake/CheckCoroutineSupport.cmake")
if (${PROJECT_NAME}_SUPPORTS_COROUTINES)
	list(APPEND MH_STUFF_SOURCES
		"cpp/include/mh/coroutine/coroutine_common.hpp"
		"cpp/include/mh/coroutine/future.hpp"
		"cpp/include/mh/coroutine/generator.hpp"
		"cpp/include/mh/coroutine/task.hpp"
		"cpp/include/mh/coroutine/thread.hpp"
		"cpp/include/mh/coroutine/thread.inl"
	)

	set(MH_COMPILE_OPTIONS ${MH_COMPILE_OPTIONS} ${COROUTINES_FLAG})
endif()

set(MH_INCLUDE_DIRS "cpp/include")
set(MH_COMPILE_FEATURES "cxx_std_20")

if (MH_STUFF_COMPILE_LIBRARY)
	add_library(mh_stuff STATIC
		${MH_STUFF_SOURCES}
		"cpp/src/io/file.cpp"
		"cpp/src/text/case_insensitive_string.cpp"
		"cpp/src/text/codecvt.cpp"
		"cpp/src/text/string_insertion.cpp"
		"cpp/src/source_location.cpp"
	)

	target_compile_definitions(mh_stuff PUBLIC
		"MH_COMPILE_LIBRARY"
		"MH_COMPILE_LIBRARY_INLINE="
	)

	# Create a cpp file of all .inl files and add it to the target
	file(GLOB_RECURSE MH_INL_FILES "*.inl")
	# message("MH_INL_FILES = ${MH_INL_FILES}")

	file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/library.cpp")
	foreach(X IN LISTS MH_INL_FILES)
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/library.cpp" "\n#include \"${X}\"")
	endforeach()
	target_sources(mh_stuff PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/library.cpp")

	target_include_directories(mh_stuff PUBLIC ${MH_INCLUDE_DIRS})
	target_compile_features(mh_stuff PUBLIC ${MH_COMPILE_FEATURES})
	target_compile_options(mh_stuff PUBLIC ${MH_COMPILE_OPTIONS})

	if (MSVC)
		target_compile_options(mh_stuff PRIVATE "/W3")
	endif()

else()
	add_library(mh_stuff INTERFACE
		${MH_STUFF_SOURCES}
	)

	target_compile_definitions(mh_stuff INTERFACE
		"MH_COMPILE_LIBRARY_INLINE=inline"
	)

	target_include_directories(mh_stuff INTERFACE ${MH_INCLUDE_DIRS})
	target_compile_features(mh_stuff INTERFACE ${MH_COMPILE_FEATURES})
	target_compile_options(mh_stuff INTERFACE ${MH_COMPILE_OPTIONS})
endif()

message("CMAKE_CXX_COMPILER_VERSION = ${CMAKE_CXX_COMPILER_VERSION}")

add_library(mh::stuff ALIAS mh_stuff)

include(CTest)
add_subdirectory("test")
