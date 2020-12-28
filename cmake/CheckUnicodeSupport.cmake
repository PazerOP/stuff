cmake_minimum_required(VERSION 3.16.3)

include(CheckCXXSourceCompiles)

function(check_cxx_unicode_support IS_SUPPORTED)

	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_REQUIRED_FLAGS "-stdlib=libc++ -lc++abi")

	check_cxx_source_compiles("
		#include <fstream>
		#include <string>

		int main(int argc, char** argv)
		{
			std::basic_ifstream<char16_t> c16;
			std::basic_string<char16_t> s16;
			std::basic_ifstream<char32_t> c32;
			std::basic_string<char32_t> s16;
		}
		" ${IS_SUPPORTED})

		message("check_cxx_unicode_support IS_SUPPORTED = ${${IS_SUPPORTED}}")

endfunction()
