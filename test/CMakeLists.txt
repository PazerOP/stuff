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
mh_test(algorithm_algorithm_generic_test)
mh_test(algorithm_multi_compare_test)
mh_test(chrono_helpers_test)
mh_test(concurrency_dispatcher_test)
mh_test(concurrency_locked_value_test)
mh_test(concurrency_main_thread_test)
mh_test(concurrency_thread_pool_test)
mh_test(concurrency_thread_sentinel_test)
mh_test(containers_heap_test)
mh_test(coroutine_generator_test)
mh_test(coroutine_task_test)
mh_test(coroutine_thread_test)
mh_test(data_bit_float_test)
mh_test(data_bits_test)
mh_test(data_lazy_test)
mh_test(data_optional_ref_test)
mh_test(data_variable_pusher_test)
mh_test(error_ensure_test)
mh_test(error_error_code_exception_test)
mh_test(error_exception_details_test)
mh_test(error_expected_test)
mh_test(error_nested_exception_test)
mh_test(error_not_implemented_error_test)
mh_test(error_status_test)
mh_test(future_test)
mh_test(http_client_test)
mh_test(io_fd_test)
mh_test(io_file_test)
mh_test(io_filesystem_helpers_test)
mh_test(math_angles_test)
mh_test(math_interpolation_test)
mh_test(math_random_test)
mh_test(math_uint128_test)
mh_test(memory_buffer_test)
mh_test(memory_cached_variable_test)
mh_test(memory_checked_ptr_test)
mh_test(memory_helpers_test)
mh_test(memory_unique_object_test)
mh_test(process_process_test)
mh_test(raii_scope_exit_test)
mh_test(reflection_enum_test)
mh_test(reflection_struct_test)
mh_test(source_location_test)
mh_test(text_case_insensitive_string_test)
mh_test(text_codecvt_test)
mh_test(text_charconv_helper_test)
mh_test(text_filebuf_test)
mh_test(text_fmtstr_test)
mh_test(text_indenting_ostream_test)
mh_test(text_insertion_conversion_test)
mh_test(text_memstream_seekdir_test)
mh_test(text_memstream_test)
mh_test(text_multi_char_test)
mh_test(text_string_insertion_test)
mh_test(text_stringops_test)
mh_test(types_enum_class_bit_ops_test)
mh_test(types_test)
mh_test(utility_test)
mh_test(variant_test)

# text_format_test gets a second, test-case-free translation unit: formatting a
# source_location from more than one TU must compile AND link in both
# header-only and library builds (was an undefined-reference regression).
mh_test(text_format_test)
target_sources(text_format_test PRIVATE text_format_link_tu.cpp)

# Multi-TU link test. Deliberately does NOT link mh::stuff: it must build with
# only the include directory, so that the build system's injected defines
# (e.g. MH_STUFF_API=) and compiled-library mode cannot mask header-only-mode
# bugs (missing MH_COMPILE_LIBRARY_INLINE markers -> multiple-definition link
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
