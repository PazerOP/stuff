cmake_minimum_required(VERSION 3.16.3)

project(
	mh-stuff
	VERSION 1.0
	DESCRIPTION "stuff that i keep re-writing"
	HOMEPAGE_URL "https://github.com/PazerOP/stuff"
	LANGUAGES CXX
)

option(MH_STUFF_BUILD_SHARED_LIBS "Build stuff as a shared lib" OFF)
if (MH_STUFF_BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
endif()

include("cmake/CheckCoroutineSupport.cmake")
include("cmake/CheckUnicodeSupport.cmake")
include(CheckCXXCompilerFlag)
include(GenerateExportHeader)

option(MH_STUFF_COMPILE_LIBRARY "True to compile some code into to a static library for less overhead" ON)
if (MH_STUFF_COMPILE_LIBRARY)
	set(MH_PUBLIC_OR_INTERFACE "PUBLIC")
	set(MH_INTERFACE_OR_EMPTY "")
else()
	set(MH_PUBLIC_OR_INTERFACE "INTERFACE")
	set(MH_INTERFACE_OR_EMPTY "INTERFACE")
endif()

add_library(stuff ${MH_INTERFACE_OR_EMPTY}
	"cpp/include/mh/algorithm/algorithm.hpp"
	"cpp/include/mh/algorithm/multi_compare.hpp"

	"cpp/include/mh/chrono/chrono_helpers.hpp"
	"cpp/include/mh/chrono/chrono_helpers.inl"

	"cpp/include/mh/concurrency/async.hpp"
	"cpp/include/mh/concurrency/dispatcher.hpp"
	"cpp/include/mh/concurrency/dispatcher.inl"
	"cpp/include/mh/concurrency/locked_value.hpp"
	"cpp/include/mh/concurrency/main_thread.hpp"
	"cpp/include/mh/concurrency/mutex_debug.hpp"
	"cpp/include/mh/concurrency/thread_pool.hpp"
	"cpp/include/mh/concurrency/thread_pool.inl"
	"cpp/include/mh/concurrency/thread_sentinel.hpp"
	"cpp/include/mh/concurrency/thread_sentinel.inl"

	"cpp/include/mh/containers/heap.hpp"

	"cpp/include/mh/coroutine/coroutine_include.hpp"
	"cpp/include/mh/coroutine/future.hpp"
	"cpp/include/mh/coroutine/generator.hpp"
	"cpp/include/mh/coroutine/task.hpp"
	"cpp/include/mh/coroutine/thread.hpp"
	"cpp/include/mh/coroutine/thread.inl"

	"cpp/include/mh/data/bit_float.hpp"
	"cpp/include/mh/data/bits.hpp"
	"cpp/include/mh/data/lazy.hpp"
	"cpp/include/mh/data/variable_pusher.hpp"

	"cpp/include/mh/error/ensure.inl"
	"cpp/include/mh/error/ensure.hpp"
	"cpp/include/mh/error/error_code_exception.hpp"
	"cpp/include/mh/error/exception_details.hpp"
	"cpp/include/mh/error/exception_details.inl"
	"cpp/include/mh/error/expected.hpp"
	"cpp/include/mh/error/nested_exception.hpp"
	"cpp/include/mh/error/not_implemented_error.hpp"
	"cpp/include/mh/error/status.hpp"

	"cpp/include/mh/io/file.hpp"
	"cpp/include/mh/io/filesystem_helpers.hpp"
	"cpp/include/mh/io/filesystem_helpers.inl"
	"cpp/include/mh/io/getopt.hpp"

	"cpp/include/mh/math/angles.hpp"
	"cpp/include/mh/math/interpolation.hpp"
	"cpp/include/mh/math/random.hpp"
	"cpp/include/mh/math/random.inl"
	"cpp/include/mh/math/uint128.hpp"

	"cpp/include/mh/memory/buffer.inl"
	"cpp/include/mh/memory/buffer.hpp"
	"cpp/include/mh/memory/cached_variable.hpp"
	"cpp/include/mh/memory/checked_ptr.hpp"
	"cpp/include/mh/memory/memory_helpers.hpp"
	"cpp/include/mh/memory/stack_info.hpp"
	"cpp/include/mh/memory/stack_info.inl"
	"cpp/include/mh/memory/unique_object.hpp"

	"cpp/include/mh/raii/scope_exit.hpp"

	"cpp/include/mh/reflection/enum.hpp"
	"cpp/include/mh/reflection/struct.hpp"

	"cpp/include/mh/text/formatters/error_code.hpp"
	"cpp/include/mh/text/case_insensitive_string.hpp"
	"cpp/include/mh/text/charconv_helper.hpp"
	"cpp/include/mh/text/codecvt.hpp"
	"cpp/include/mh/text/codecvt.inl"
	"cpp/include/mh/text/fmtstr.hpp"
	"cpp/include/mh/text/format.hpp"
	"cpp/include/mh/text/indenting_ostream.hpp"
	"cpp/include/mh/text/insertion_conversion.hpp"
	"cpp/include/mh/text/memstream.hpp"
	"cpp/include/mh/text/string_insertion.hpp"
	"cpp/include/mh/text/stringops.hpp"

	"cpp/include/mh/types/disable_copy_move.hpp"
	"cpp/include/mh/types/enum_class_bit_ops.hpp"

	"cpp/include/mh/compiler.hpp"
	"cpp/include/mh/future.hpp"
	"cpp/include/mh/source_location.hpp"
	"cpp/include/mh/utility.hpp"
	"cpp/include/mh/variant.hpp"
)

set_target_properties(stuff PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

if (MH_STUFF_COMPILE_LIBRARY)
	target_sources(stuff PRIVATE
		"cpp/src/io/file.cpp"
		"cpp/src/text/case_insensitive_string.cpp"
		"cpp/src/text/string_insertion.cpp"
		"cpp/src/source_location.cpp"
	)

	target_compile_definitions(stuff ${MH_PUBLIC_OR_INTERFACE}
		"MH_COMPILE_LIBRARY"
		"MH_COMPILE_LIBRARY_INLINE="
	)

	# Create a cpp file of all .inl files and add it to the target
	set(MH_CPP_INCLUDE_ROOT "${PROJECT_SOURCE_DIR}/cpp/include")
	file(GLOB_RECURSE MH_INL_FILES "${MH_CPP_INCLUDE_ROOT}/mh/*.inl")
	# message("MH_INL_FILES = ${MH_INL_FILES}")

	file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/library.cpp")
	foreach(X IN LISTS MH_INL_FILES)
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/library.cpp" "\n#include \"${X}\"")
	endforeach()
	target_sources(stuff PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/library.cpp")

	if (MSVC)
		target_compile_options(stuff PRIVATE /W3 /permissive-)
	endif()

	generate_export_header(stuff
		BASE_NAME ${PROJECT_NAME}
		EXPORT_MACRO_NAME MH_STUFF_API
	)
	target_precompile_headers(stuff ${MH_PUBLIC_OR_INTERFACE} "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_export.h")

else()
	target_compile_definitions(stuff INTERFACE
		"MH_COMPILE_LIBRARY_INLINE=inline"
		"MH_STUFF_API="
	)
endif()

check_cxx_coroutine_support(SUPPORTS_COROUTINES COROUTINES_FLAGS)
target_compile_options(stuff ${MH_PUBLIC_OR_INTERFACE} ${COROUTINES_FLAGS})

target_include_directories(stuff ${MH_PUBLIC_OR_INTERFACE}
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(stuff ${MH_PUBLIC_OR_INTERFACE} "cxx_std_20")

if (MSVC)
	# There is a bug in visual studio that prevents intellisense from realizing
	# /std:c++latest is on the command line if you only use target_compile_features(cxx_std_20)
	target_compile_options(stuff ${MH_PUBLIC_OR_INTERFACE} "/std:c++latest")
elseif (CMAKE_CXX_COMPILER_ID MATCHES GNU)
	target_link_libraries(stuff ${MH_PUBLIC_OR_INTERFACE} stdc++fs)
elseif (CMAKE_CXX_COMPILER_ID MATCHES Clang)
	target_compile_options(stuff ${MH_PUBLIC_OR_INTERFACE} -stdlib=libc++)
	target_link_options(stuff ${MH_PUBLIC_OR_INTERFACE} -stdlib=libc++)
	target_link_libraries(stuff ${MH_PUBLIC_OR_INTERFACE} c++abi)

	if ((CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9"))
		target_link_libraries(stuff ${MH_PUBLIC_OR_INTERFACE} c++fs)
	endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
	target_compile_options(stuff ${MH_PUBLIC_OR_INTERFACE} -pthread)
	target_link_options(stuff ${MH_PUBLIC_OR_INTERFACE} -pthread)
endif()

if (NOT MSVC)
	check_cxx_compiler_flag(-fconcepts FCONCEPTS_FLAG)
	if (FCONCEPTS_FLAG)
		target_compile_options(stuff ${MH_PUBLIC_OR_INTERFACE} -fconcepts)
	endif()
endif()

check_cxx_unicode_support(SUPPORTS_UNICODE stuff)
if (NOT SUPPORTS_UNICODE)
	target_compile_definitions(stuff ${MH_PUBLIC_OR_INTERFACE} "MH_BROKEN_UNICODE")
endif()

add_library(mh::stuff ALIAS stuff)

if (NOT DEFINED BUILD_TESTING)
	if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
		set(BUILD_TESTING ON)
	else()
		set(BUILD_TESTING OFF)
	endif()
	message("BUILD_TESTING not defined, changing to ${BUILD_TESTING}")
else()
	message("BUILD_TESTING = ${BUILD_TESTING}, not modifying")
endif()

if (BUILD_TESTING)
	include(CTest)
	add_subdirectory("test")
endif()

###########################################
# "install" is intended for vcpkg support #
###########################################
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

configure_package_config_file(
	cmake/config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)

install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
	DESTINATION
		"${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)

install(TARGETS stuff EXPORT ${PROJECT_NAME}_targets)
install(DIRECTORY cpp/include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(
	EXPORT ${PROJECT_NAME}_targets
	NAMESPACE mh::
	DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)
