#include "mh/error/error_code_exception.hpp"

#include <catch2/catch_all.hpp>

#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

TEST_CASE("error_code_exception - type aliases", "[error][error_code_exception]")
{
	STATIC_CHECK(std::is_base_of_v<std::system_error, mh::error_condition_exception>);
	STATIC_CHECK(std::is_same_v<mh::error_condition_exception::base_type, std::system_error>);
	STATIC_CHECK(std::is_same_v<mh::error_condition_exception::code_type, std::error_condition>);
	STATIC_CHECK(std::is_same_v<mh::error_condition_exception, mh::basic_error_code_exception<>>);
}

TEST_CASE("error_code_exception - round-trips an std::error_condition", "[error][error_code_exception]")
{
	const auto condition = std::make_error_condition(std::errc::invalid_argument);
	const mh::error_condition_exception ex(condition);

	const auto roundTripped = ex.code();
	CHECK(roundTripped == condition);
	CHECK(roundTripped.value() == condition.value());
	CHECK(roundTripped.category() == condition.category());
}

TEST_CASE("error_code_exception - message is part of what()", "[error][error_code_exception]")
{
	SECTION("with a custom message")
	{
		const mh::error_condition_exception ex(
			std::make_error_condition(std::errc::not_supported), "while doing the thing");

		CHECK(std::string_view(ex.what()).find("while doing the thing") != std::string_view::npos);
	}
	SECTION("with the default (empty) message")
	{
		const mh::error_condition_exception ex(std::make_error_condition(std::errc::not_supported));
		CHECK(ex.what() != nullptr);
	}
}

TEST_CASE("error_code_exception - works with std::error_code too", "[error][error_code_exception]")
{
	using error_code_exception = mh::basic_error_code_exception<std::error_code>;
	STATIC_CHECK(std::is_same_v<error_code_exception::code_type, std::error_code>);

	const auto code = std::make_error_code(std::errc::timed_out);
	const error_code_exception ex(code, "deadline");

	CHECK(ex.code() == code);
	CHECK(ex.code().value() == code.value());
	CHECK(ex.code().category() == code.category());
}

TEST_CASE("error_code_exception - catchable as std::system_error", "[error][error_code_exception]")
{
	const auto condition = std::make_error_condition(std::errc::permission_denied);

	try
	{
		throw mh::error_condition_exception(condition, "denied");
	}
	catch (const std::system_error& e)
	{
		CHECK(e.code().value() == condition.value());
		CHECK(std::string_view(e.what()).find("denied") != std::string_view::npos);
	}

	CHECK_THROWS_AS(throw mh::error_condition_exception(condition), std::system_error);
}
