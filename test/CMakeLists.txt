cmake_minimum_required(VERSION 3.17)

add_subdirectory("catch2/repo")
include("catch2/repo/contrib/Catch.cmake")

# add_library(catch2_static STATIC "catch2/catch2.cpp")
# target_compile_features(catch2_static PUBLIC cxx_std_20)
target_compile_features(Catch2WithMain PUBLIC cxx_std_20)
# target_link_libraries(catch2_static Catch2::Catch2)

function(mh_test name)
	add_executable(${name} "${name}.cpp")

	target_link_libraries(${name} Catch2::Catch2WithMain mh::stuff)
	catch_discover_tests(${name})
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
if (HAS_GETOPT)
	mh_test(io_getopt_test)
endif()

# add_executable(text_stringops_test "text_stringops_test.cpp")
# target_link_libraries(text_stringops_test catch2 mh::stuff)
# add_test(NAME text_stringops_test COMMAND text_stringops_test)
