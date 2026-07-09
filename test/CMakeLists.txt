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

# Use CPM to get Catch2
CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.4.0
  GIT_TAG v3.4.0
)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)


# Coverage instrumentation. GCC only: gating this on find_library(gcov) silently
# disabled coverage everywhere (libgcov.a lives in the compiler-private directory
# that find_library never searches), and the driver links libgcov itself when
# --coverage is passed at link time. Clang is excluded: its gcov data needs the
# matching llvm-cov, and CI does not install clang's profile runtime.
if (CMAKE_CXX_COMPILER_ID MATCHES GNU)
	target_compile_options(mh-stuff ${MH_PUBLIC_OR_INTERFACE} --coverage -g)
	target_link_options(mh-stuff ${MH_PUBLIC_OR_INTERFACE} --coverage)
endif()

function(mh_test name)
	add_executable(${name} "${name}.cpp")

	target_link_libraries(${name} Catch2::Catch2WithMain mh::stuff)
	catch_discover_tests(${name} TEST_PREFIX "${PROJECT_NAME}.")

	if (CMAKE_CXX_COMPILER_ID MATCHES GNU)
		target_compile_options(${name} PRIVATE --coverage -g)
		target_link_options(${name} PRIVATE --coverage)
	endif()

endfunction()

mh_test(algorithm_algorithm_test)
mh_test(coroutine_task_test)
mh_test(data_bit_float_test)
mh_test(data_bits_test)
mh_test(data_variable_pusher_test)
mh_test(math_interpolation_test)
mh_test(math_uint128_test)
mh_test(memory_buffer_test)
mh_test(text_case_insensitive_string_test)
mh_test(text_codecvt_test)
mh_test(text_charconv_helper_test)
mh_test(text_filebuf_test)
mh_test(text_memstream_test)
mh_test(text_string_insertion_test)
mh_test(text_stringops_test)

include(CheckIncludeFileCXX)
check_include_file_cxx("getopt.h" HAS_GETOPT)
check_include_file_cxx("unistd.h" HAS_UNISTD)
if (HAS_GETOPT OR HAS_UNISTD)
	mh_test(io_getopt_test)
endif()

# add_executable(text_stringops_test "text_stringops_test.cpp")
# target_link_libraries(text_stringops_test catch2 mh::stuff)
# add_test(NAME text_stringops_test COMMAND text_stringops_test)

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
