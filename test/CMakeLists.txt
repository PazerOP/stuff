cmake_minimum_required(VERSION 3.16.3)

# Set compiler flags for clang/libc++ before adding Catch2
# This ensures Catch2 is built with the same stdlib as our code
if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

# Build Catch2 with the same C++ standard as the tests that link it. Catch2 only
# requires cxx_std_14 (a minimum), so on compilers whose default is pre-C++17
# (clang < 16) libCatch2.a would otherwise lack symbols that its headers declare
# for C++17+ consumers (e.g. Catch::StringMaker<std::string_view>::convert),
# breaking the link of any C++20 test that stringifies a string_view.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include CPM for package management
include(${PROJECT_SOURCE_DIR}/cmake/get_cpm.cmake)

enable_testing()
include(CTest)

# Use CPM to get Catch2
CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.12.0
  GIT_TAG v3.12.0
  OPTIONS
    "CATCH_CONFIG_CPP17_BYTE ON"
    "CATCH_CONFIG_CPP17_STRING_VIEW ON"
)

target_compile_features(Catch2 PUBLIC cxx_std_20)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

# Coverage instrumentation. GCC only: gating this on find_library(gcov) silently
# disabled coverage everywhere (libgcov.a lives in the compiler-private directory
# that find_library never searches), and the driver links libgcov itself when
# --coverage is passed at link time. Clang is excluded: its gcov data needs the
# matching llvm-cov, and CI does not install clang's profile runtime.
if (ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES GNU)
	target_compile_options(mh-stuff ${MH_PUBLIC_OR_INTERFACE} --coverage -g)
	target_link_options(mh-stuff ${MH_PUBLIC_OR_INTERFACE} --coverage)
endif()
endif()

# Check that all test files have last_include.hpp as the final include
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/check_last_include.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE LAST_INCLUDE_CHECK_RESULT
    OUTPUT_VARIABLE LAST_INCLUDE_CHECK_OUTPUT
    ERROR_VARIABLE LAST_INCLUDE_CHECK_ERROR
)

if(NOT LAST_INCLUDE_CHECK_RESULT EQUAL 0)
    message(FATAL_ERROR "last_include.hpp check failed:\n${LAST_INCLUDE_CHECK_OUTPUT}\n${LAST_INCLUDE_CHECK_ERROR}")
endif()

# Glob all test source files
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*_test.cpp")

# text_format_test gets a second, test-case-free translation unit: formatting a
# source_location from more than one TU must compile AND link in both
# header-only and library builds (was an undefined-reference regression).
target_sources(text_format_test PRIVATE text_format_link_tu.cpp)

# Multi-TU link test. Deliberately does NOT link mh::stuff: it must build with
# only the include directory, so that the build system's injected defines
# (e.g. MH_STUFF_API=) and compiled-library mode cannot mask header-only-mode
# bugs (missing MH_COMPILE_LIBRARY_INLINE markers, multiple-definition link
# errors, per-TU copies of program-wide entities, headers that rely on
# build-system macros). Linking this executable is the core of the test.
add_executable(multi_tu_test multi_tu_a.cpp multi_tu_b.cpp)
target_compile_features(multi_tu_test PRIVATE cxx_std_20)
target_include_directories(multi_tu_test PRIVATE "${PROJECT_SOURCE_DIR}/cpp/include")
target_link_libraries(multi_tu_test Catch2::Catch2WithMain)
if (NOT MSVC)
	target_compile_options(multi_tu_test PRIVATE -pthread)
	target_link_options(multi_tu_test PRIVATE -pthread)
endif()
catch_discover_tests(multi_tu_test TEST_PREFIX "${PROJECT_NAME}.")

# Check for getopt/unistd and conditionally exclude io_getopt_test if not available
include(CheckIncludeFileCXX)
check_include_file_cxx("getopt.h" HAS_GETOPT)
check_include_file_cxx("unistd.h" HAS_UNISTD)
if (NOT (HAS_GETOPT OR HAS_UNISTD))
	list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/io_getopt_test.cpp")
endif()
endif()

# Create the unified test executable with custom main
add_executable(mh_stuff_tests ${TEST_SOURCES} TestMain.cpp)

# Note: Custom string makers are defined in specific test files

target_link_libraries(mh_stuff_tests Catch2::Catch2 mh::stuff)
catch_discover_tests(mh_stuff_tests TEST_PREFIX "${PROJECT_NAME}.")

# Code coverage support for tests (only enable if explicitly requested)
if (ENABLE_COVERAGE AND (CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU))
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
	target_include_directories(test_compile_file_${SAFE_FILENAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()
