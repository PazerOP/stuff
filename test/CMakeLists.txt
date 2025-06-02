cmake_minimum_required(VERSION 3.16.3)

# Include CPM for package management
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CPM.cmake)

enable_testing()
include(CTest)

# Use CPM to get Catch2
CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.8.0
  GIT_TAG v3.8.0
)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

if (CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
	find_library(GCOV_LIBRARY gcov)
	if(GCOV_LIBRARY)
		target_compile_options(mh-stuff ${MH_PUBLIC_OR_INTERFACE} --coverage -g)
		target_link_options(mh-stuff ${MH_PUBLIC_OR_INTERFACE} -lgcov --coverage -g)
	endif()
endif()

# Glob all test source files
file(GLOB TEST_SOURCES "*_test.cpp")

# Check for getopt/unistd and conditionally exclude io_getopt_test if not available
include(CheckIncludeFileCXX)
check_include_file_cxx(<getopt.h> HAS_GETOPT)
check_include_file_cxx(<unistd.h> HAS_UNISTD)
if (NOT (HAS_GETOPT OR HAS_UNISTD))
	list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/io_getopt_test.cpp")
endif()

# Create the unified test executable
add_executable(mh_stuff_tests ${TEST_SOURCES})

# Note: Custom string makers are defined in specific test files

target_link_libraries(mh_stuff_tests Catch2::Catch2WithMain mh::stuff)
catch_discover_tests(mh_stuff_tests TEST_PREFIX "${PROJECT_NAME}.")

if (CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
	find_library(GCOV_LIBRARY gcov)
	if(GCOV_LIBRARY)
		target_compile_options(mh_stuff_tests PUBLIC --coverage -g)
		target_link_options(mh_stuff_tests PUBLIC -lgcov --coverage -g)
	endif()
endif()

# Make sure all header files can be compiled successfully by themselves
set(MH_CPP_INCLUDE_ROOT "${PROJECT_SOURCE_DIR}/cpp/include")
file(GLOB_RECURSE MH_HPP_FILES
	RELATIVE ${MH_CPP_INCLUDE_ROOT}
	"${MH_CPP_INCLUDE_ROOT}/mh/*.hpp"
)
foreach (F IN LISTS MH_HPP_FILES)
	string(REPLACE "/" "_" SAFE_FILENAME ${F})
	string(REPLACE "\\" "_" SAFE_FILENAME ${SAFE_FILENAME})
	string(REPLACE "." "_" SAFE_FILENAME ${SAFE_FILENAME})

	set(GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/test_compile_file/${SAFE_FILENAME}.cpp")
	set(TEST_FILE_NAME ${F})
	configure_file("${PROJECT_SOURCE_DIR}/test/test_compile_file_base.cpp" ${GENERATED_FILE})

	add_library(test_compile_file_${SAFE_FILENAME} STATIC
		"${MH_CPP_INCLUDE_ROOT}/${F}"
		"${GENERATED_FILE}"
	)

	target_link_libraries(test_compile_file_${SAFE_FILENAME} PUBLIC mh::stuff)
endforeach()
