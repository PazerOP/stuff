cmake_minimum_required(VERSION 3.16.3)

add_subdirectory("catch2/repo")
include("catch2/repo/contrib/Catch.cmake")

# add_library(catch2_static STATIC "catch2/catch2.cpp")
# target_compile_features(catch2_static PUBLIC cxx_std_20)
target_compile_features(Catch2WithMain PUBLIC cxx_std_20)
# target_link_libraries(catch2_static Catch2::Catch2)

# build with correct stdlib for complier
if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
	target_compile_options(Catch2WithMain PUBLIC -stdlib=libc++)
	target_link_options(Catch2WithMain PUBLIC -stdlib=libc++ -lc++abi)
endif()

function(mh_test name)
	add_executable(${name} "${name}.cpp")

	target_link_libraries(${name} Catch2::Catch2WithMain mh::stuff)
	catch_discover_tests(${name} TEST_PREFIX "${PROJECT_NAME}.")
endfunction()

mh_test(algorithm_algorithm_test)
mh_test(data_bit_float_test)
mh_test(data_bits_test)
mh_test(data_variable_pusher_test)
mh_test(math_interpolation_test)
mh_test(math_uint128_test)
mh_test(memory_buffer_test)
mh_test(text_case_insensitive_string_test)
# mh_test(text_charconv_helper_test)
mh_test(text_memstream_test)
mh_test(text_string_insertion_test)
mh_test(text_stringops_test)

include(CheckIncludeFileCXX)
check_include_file_cxx(<getopt.h> HAS_GETOPT)
if (NOT HAS_GETOPT)
	check_include_file_cxx(<unistd.h> HAS_GETOPT)
endif()
if (HAS_GETOPT)
	mh_test(io_getopt_test)
endif()

# add_executable(text_stringops_test "text_stringops_test.cpp")
# target_link_libraries(text_stringops_test catch2 mh::stuff)
# add_test(NAME text_stringops_test COMMAND text_stringops_test)

# Make sure all header files can be compiled successfully by themselves

set(MH_CPP_INCLUDE_ROOT "${PROJECT_SOURCE_DIR}/cpp/include")
file(GLOB_RECURSE MH_HPP_FILES RELATIVE ${MH_CPP_INCLUDE_ROOT} "${MH_CPP_INCLUDE_ROOT}/mh/*.hpp")
message("MH_HPP_FILES = ${MH_HPP_FILES}")
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
