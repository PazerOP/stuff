cmake_minimum_required(VERSION 3.16.3)

find_package(Catch2 CONFIG REQUIRED)
include("${Catch2_DIR}/Catch.cmake")

add_library(Catch2WithMain STATIC "catch2.cpp")
target_link_libraries(Catch2WithMain PUBLIC Catch2::Catch2)
target_compile_features(Catch2WithMain PUBLIC cxx_std_20)

# build with correct stdlib for complier
if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
	target_compile_options(Catch2WithMain PUBLIC -stdlib=libc++)
	target_link_options(Catch2WithMain PUBLIC -stdlib=libc++ -lc++abi)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
	target_compile_options(stuff ${MH_PUBLIC_OR_INTERFACE} --coverage -g)
	target_link_options(stuff ${MH_PUBLIC_OR_INTERFACE} -lgcov --coverage -g)
endif()

function(mh_test name)
	add_executable(${name} "${name}.cpp")

	target_link_libraries(${name} Catch2WithMain mh::stuff)
	catch_discover_tests(${name} TEST_PREFIX "${PROJECT_NAME}.")

	if (CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
		target_compile_options(${name} PUBLIC --coverage -g)
		target_link_options(${name} PUBLIC -lgcov --coverage -g)
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
# mh_test(text_charconv_helper_test)
mh_test(text_memstream_test)
mh_test(text_string_insertion_test)
mh_test(text_stringops_test)

include(CheckIncludeFileCXX)
check_include_file_cxx(<getopt.h> HAS_GETOPT)
check_include_file_cxx(<unistd.h> HAS_UNISTD)
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
