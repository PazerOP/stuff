#include "mh/source_location.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <string>
#include "last_include.hpp"

TEST_CASE("source_location basic functionality", "[source_location]")
{
	SECTION("default construction")
	{
		mh::source_location loc;
		REQUIRE(loc.line() == 0);
		REQUIRE(loc.column() == 0);
#if !__cpp_lib_source_location
		// Only test nullptr behavior for custom implementation
		REQUIRE(loc.file_name() == nullptr);
		REQUIRE(loc.function_name() == nullptr);
#else
		// std::source_location may return empty strings instead of nullptr
		// Just verify that file_name() and function_name() are callable
		auto file_name = loc.file_name();
		auto func_name = loc.function_name();
		(void)file_name; // Suppress unused variable warning
		(void)func_name; // Suppress unused variable warning
#endif
	}

	SECTION("explicit construction")
	{
#if !__cpp_lib_source_location
		// Only test explicit construction if using our custom implementation
		const char* file = "test_file.cpp";
		const char* function = "test_function";
		constexpr std::uint_least32_t line = 42;
		constexpr std::uint_least32_t column = 10;
		
		mh::source_location loc(line, file, function, column);
		REQUIRE(loc.line() == line);
		REQUIRE(loc.column() == column);
		REQUIRE(loc.file_name() == file);
		REQUIRE(loc.function_name() == function);
#endif
	}

	SECTION("explicit construction without column")
	{
#if !__cpp_lib_source_location
		// Only test explicit construction if using our custom implementation
		const char* file = "test_file.cpp";
		const char* function = "test_function";
		constexpr std::uint_least32_t line = 100;
		
		mh::source_location loc(line, file, function);
		REQUIRE(loc.line() == line);
		REQUIRE(loc.column() == 0);
		REQUIRE(loc.file_name() == file);
		REQUIRE(loc.function_name() == function);
#endif
	}
}

TEST_CASE("source_location current() function", "[source_location]")
{
#if defined(MH_SOURCE_LOCATION_CURRENT)
	SECTION("current() returns valid location")
	{
		auto loc = MH_SOURCE_LOCATION_CURRENT();
		
		// Basic validity checks
		REQUIRE(loc.line() > 0); // Should be a valid line number
		REQUIRE(loc.file_name() != nullptr);
		REQUIRE(loc.function_name() != nullptr);
		
		// Check that file name contains this file's name
		std::string file_name(loc.file_name());
		REQUIRE(file_name.find("source_location_test") != std::string::npos);
	}

	SECTION("current() captures correct line numbers")
	{
		auto loc1 = MH_SOURCE_LOCATION_CURRENT(); auto line1 = __LINE__;
		auto loc2 = MH_SOURCE_LOCATION_CURRENT(); auto line2 = __LINE__;
		
		// The captured line should be close to the actual line
		REQUIRE(std::abs(static_cast<int>(loc1.line()) - static_cast<int>(line1)) <= 1);
		REQUIRE(std::abs(static_cast<int>(loc2.line()) - static_cast<int>(line2)) <= 1);
		
		// loc2 should be after loc1
		REQUIRE(loc2.line() > loc1.line());
	}
#endif
}

auto get_location_from_function() -> mh::source_location
{
#if defined(MH_SOURCE_LOCATION_CURRENT)
	return MH_SOURCE_LOCATION_CURRENT();
#elif !__cpp_lib_source_location
	return mh::source_location(__LINE__, __FILE__, __func__);
#else
	return mh::source_location::current();
#endif
}

TEST_CASE("source_location function name capture", "[source_location]")
{
	SECTION("captures function name correctly")
	{
		auto loc = get_location_from_function();
		
		REQUIRE(loc.function_name() != nullptr);
		std::string func_name(loc.function_name());
		REQUIRE(func_name.find("get_location_from_function") != std::string::npos);
	}
}

TEST_CASE("source_location stream insertion", "[source_location]")
{
#if !__cpp_lib_source_location
	// Only test stream insertion if using our custom implementation
	SECTION("stream insertion operator")
	{
		const char* file = "test.cpp";
		const char* function = "test_func";
		constexpr std::uint_least32_t line = 123;
		
		mh::source_location loc(line, file, function);
		
		std::ostringstream oss;
		oss << loc;
		
		std::string result = oss.str();
		REQUIRE(result.find("test.cpp") != std::string::npos);
		REQUIRE(result.find("123") != std::string::npos);
		REQUIRE(result.find("test_func") != std::string::npos);
		REQUIRE(result.find("(") != std::string::npos);
		REQUIRE(result.find(")") != std::string::npos);
		REQUIRE(result.find(":") != std::string::npos);
	}

	SECTION("stream insertion with default location")
	{
		mh::source_location loc;
		
		std::ostringstream oss;
		oss << loc;
		
		// Should not crash with default-constructed location
		std::string result = oss.str();
		REQUIRE(!result.empty()); // Should produce some output
	}
#endif
}

TEST_CASE("source_location macro usage", "[source_location]")
{
#if defined(MH_SOURCE_LOCATION_AUTO)
	SECTION("MH_SOURCE_LOCATION_AUTO macro")
	{
		// This should compile and work
		auto test_lambda = [](MH_SOURCE_LOCATION_AUTO(loc)) {
			return loc;
		};
		
#if defined(MH_SOURCE_LOCATION_CURRENT)
		auto result = test_lambda();
		REQUIRE(result.line() > 0);
		REQUIRE(result.file_name() != nullptr);
		REQUIRE(result.function_name() != nullptr);
#endif
	}
#endif
}

TEST_CASE("source_location constexpr functionality", "[source_location]")
{
	SECTION("constexpr construction and access")
	{
#if !__cpp_lib_source_location
		// Only test explicit construction if using our custom implementation
		constexpr mh::source_location loc(42, "test.cpp", "test_func", 10);
		
		static_assert(loc.line() == 42);
		static_assert(loc.column() == 10);
		static_assert(loc.file_name() != nullptr);
		static_assert(loc.function_name() != nullptr);
		
		REQUIRE(loc.line() == 42);
		REQUIRE(loc.column() == 10);
		REQUIRE(std::string(loc.file_name()) == "test.cpp");
		REQUIRE(std::string(loc.function_name()) == "test_func");
#endif
	}

	SECTION("constexpr default construction")
	{
		constexpr mh::source_location loc;
		
#if !__cpp_lib_source_location
		// Only test specific values if using our custom implementation
		static_assert(loc.line() == 0);
		static_assert(loc.column() == 0);
		static_assert(loc.file_name() == nullptr);
		static_assert(loc.function_name() == nullptr);
		
		REQUIRE(loc.line() == 0);
		REQUIRE(loc.column() == 0);
		REQUIRE(loc.file_name() == nullptr);
		REQUIRE(loc.function_name() == nullptr);
#else
		// For std::source_location, just test that it compiles
		REQUIRE(loc.line() >= 0);
#endif
	}
}