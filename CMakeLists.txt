cmake_minimum_required(VERSION 3.16.3)

# Enable compile_commands.json generation for this subproject
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

option(MH_STUFF_COMPILE_LIBRARY "True to compile some code into to a static library for less overhead" ON)
if (MH_STUFF_COMPILE_LIBRARY)
	set(MH_PUBLIC_OR_INTERFACE "PUBLIC")
	set(MH_INTERFACE_OR_EMPTY "")
else()
	set(MH_PUBLIC_OR_INTERFACE "INTERFACE")
	set(MH_INTERFACE_OR_EMPTY "INTERFACE")
endif()

file(GLOB_RECURSE MH_HEADER_FILES CONFIGURE_DEPENDS
	"cpp/include/mh/*.hpp"
	"cpp/include/mh/*.inl"
)

add_library(${PROJECT_NAME} ${MH_INTERFACE_OR_EMPTY}
	${MH_HEADER_FILES}
)
add_library(mh::stuff ALIAS ${PROJECT_NAME})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mh-cmake-common")
include(mh-BasicInstall)
include(mh-CheckCoroutineSupport)

if (MH_STUFF_COMPILE_LIBRARY)
	find_package(CURL REQUIRED)

	file(GLOB_RECURSE MH_SOURCE_FILES CONFIGURE_DEPENDS
		"cpp/src/*.cpp"
	)
	target_sources(${PROJECT_NAME} PRIVATE
		${MH_SOURCE_FILES}
	)

	target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)

	target_compile_definitions(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE}
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
	target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/library.cpp")

	if (MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /W3 /permissive- /utf-8)
	endif()

	include(GenerateExportHeader)
	generate_export_header(${PROJECT_NAME}
		BASE_NAME ${PROJECT_NAME}
		EXPORT_MACRO_NAME MH_STUFF_API
	)
	# PRIVATE: a PUBLIC PCH exports INTERFACE_PRECOMPILE_HEADERS with an absolute
	# build-tree path into the installed target (and the export header is never
	# installed), breaking every consumer on a machine without this build tree.
	# The headers all carry an #ifndef MH_STUFF_API fallback for consumers.
	target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_export.h")

else()
	target_compile_definitions(${PROJECT_NAME} INTERFACE
		"MH_COMPILE_LIBRARY_INLINE=inline"
		"MH_STUFF_API="
	)
endif()

mh_check_cxx_coroutine_support(SUPPORTS_COROUTINES COROUTINES_FLAGS)
target_compile_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} ${COROUTINES_FLAGS})

# Optional fmt support: the vcpkg manifest has always shipped fmt, but nothing
# ever put its headers on the include path, so text/format.hpp's
# __has_include(<fmt/format.h>) was false and the whole fmt-backed formatter
# layer silently compiled as MH_FORMATTER_NONE (untested, unusable). Link it
# when present AND actually linkable: a found fmt can still be ABI-incompatible
# (e.g. a libstdc++-built libfmt while this target uses clang + libc++), so
# probe a real compile+link of a compiled-fmt symbol before committing to it.
# On probe failure we fall back to MH_FORMATTER_NONE - the status quo.
find_package(fmt CONFIG QUIET)
set(MH_STUFF_USE_FMT FALSE)
if (fmt_FOUND)
	include(CheckCXXSourceCompiles)
	set(CMAKE_REQUIRED_LIBRARIES fmt::fmt)
	if (MSVC)
		set(CMAKE_REQUIRED_FLAGS "/std:c++20")
	elseif (CMAKE_CXX_COMPILER_ID MATCHES Clang)
		set(CMAKE_REQUIRED_FLAGS "-std=c++20 -stdlib=libc++")
	else()
		set(CMAKE_REQUIRED_FLAGS "-std=c++20")
	endif()
	check_cxx_source_compiles("
#include <fmt/format.h>
int main() { return static_cast<int>(fmt::format(\"{}\", 42).size()) - 2; }
" MH_STUFF_FMT_LINKS)
	set(CMAKE_REQUIRED_LIBRARIES "")
	set(CMAKE_REQUIRED_FLAGS "")

	if (MH_STUFF_FMT_LINKS)
		set(MH_STUFF_USE_FMT TRUE)
	endif()
endif()

if (MH_STUFF_USE_FMT)
	message(STATUS "fmt found (${fmt_VERSION}): mh::format will use fmtlib")
	target_link_libraries(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} fmt::fmt)
elseif (fmt_FOUND)
	message(STATUS "fmt found (${fmt_VERSION}) but not linkable with this toolchain; mh::format falls back to MH_FORMATTER_NONE")
endif()

target_include_directories(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE}
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} "cxx_std_20")

if (MSVC)
	# There is a bug in visual studio that prevents intellisense from realizing
	# /std:c++latest is on the command line if you only use target_compile_features(cxx_std_20)
	target_compile_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} "/std:c++latest")
elseif (CMAKE_CXX_COMPILER_ID MATCHES GNU)
	target_link_libraries(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} stdc++fs)
elseif (CMAKE_CXX_COMPILER_ID MATCHES Clang)
	target_compile_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} -stdlib=libc++)
	target_link_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} -stdlib=libc++)
	target_link_libraries(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} c++abi)

	if ((CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9"))
		target_link_libraries(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} c++fs)
	endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
	target_compile_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} -pthread)
	target_link_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} -pthread)
endif()

# NOTE: __unix__ is deliberately NOT defined here. It is a compiler-reserved
# identifier that every Unix compiler predefines itself; force-defining it
# PUBLIC/INTERFACE injected it into every consumer TU (and, being exported,
# flipped platform detection of unrelated third-party headers on platforms
# like macOS that do not predefine it).

if (NOT MSVC)
	include(CheckCXXCompilerFlag)
	check_cxx_compiler_flag(-fconcepts FCONCEPTS_FLAG)
	if (FCONCEPTS_FLAG)
		target_compile_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} -fconcepts)
	endif()
endif()

# Force enable MH_BROKEN_UNICODE for the current system
target_compile_definitions(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} "MH_BROKEN_UNICODE=1")
message(STATUS "Forcing MH_BROKEN_UNICODE=1 to work around compiler Unicode issues")

if (NOT DEFINED BUILD_TESTING)
	if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
		set(BUILD_TESTING ON)
	else()
		set(BUILD_TESTING OFF)
	endif()
else()
endif()

if (BUILD_TESTING)
	include(CTest)
	add_subdirectory("test")
endif()

# The compiled library's exported link interface references CURL::libcurl
# ($<LINK_ONLY:...>), so the installed package config must find_dependency(CURL)
# or consumer find_package(mh-stuff) fails at generate time. Plain variable, not
# a genexp: configure_package_config_file cannot evaluate generator expressions.
if (MH_STUFF_COMPILE_LIBRARY)
	set(MH_STUFF_INSTALL_DEPS CURL)
endif()
if (MH_STUFF_USE_FMT)
	list(APPEND MH_STUFF_INSTALL_DEPS fmt)
endif()

mh_basic_install(
	PROJ_INCLUDE_DIRS "cpp/include/"
	FIND_DEPENDENCIES ${MH_STUFF_INSTALL_DEPS}
)

###########################################
# "install" is intended for vcpkg support #
###########################################
# include(CMakePackageConfigHelpers)
# configure_package_config_file(
# 	cmake/config.cmake.in
# 	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
# 	INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
# )

# include(GNUInstallDirs)

# write_basic_package_version_file(
# 	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
# 	VERSION ${PROJECT_VERSION}
# 	COMPATIBILITY SameMajorVersion
# )

# install(
# 	FILES
# 		"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
# 		"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
# 	DESTINATION
# 		"${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
# )

# install(TARGETS stuff EXPORT ${PROJECT_NAME}_targets)
# install(DIRECTORY cpp/include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
# install(
# 	EXPORT ${PROJECT_NAME}_targets
# 	NAMESPACE mh::
# 	DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
# )
