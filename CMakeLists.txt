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
	target_precompile_headers(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_export.h")

else()
	target_compile_definitions(${PROJECT_NAME} INTERFACE
		"MH_COMPILE_LIBRARY_INLINE=inline"
		"MH_STUFF_API="
	)
endif()

mh_check_cxx_coroutine_support(SUPPORTS_COROUTINES COROUTINES_FLAGS)
target_compile_options(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} ${COROUTINES_FLAGS})

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

# Define __unix__ for Unix-like systems (Linux, macOS, etc.)
if (UNIX)
	target_compile_definitions(${PROJECT_NAME} ${MH_PUBLIC_OR_INTERFACE} "__unix__")
endif()

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

mh_basic_install(
	PROJ_INCLUDE_DIRS "cpp/include/"
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
