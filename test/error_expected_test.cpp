#include "mh/error/expected.hpp"
#include <catch2/catch_all.hpp>

#include <compare>
#include <memory>

TEST_CASE("expected - basic value/error state", "[error][expected]")
{
	mh::expected<int, long> hasValue(mh::expect, 5);
	REQUIRE(hasValue.has_value());
	REQUIRE(!hasValue.has_error());
	REQUIRE(hasValue.value() == 5);

	mh::expected<int, long> hasError(mh::unexpect, 3L);
	REQUIRE(!hasError.has_value());
	REQUIRE(hasError.has_error());
	REQUIRE(hasError.error() == 3);
}

// mirrors the feature guard around expected's operator<=> overloads
#if (__cpp_lib_three_way_comparison >= 201907) || (_MSC_VER >= 1928)
TEST_CASE("expected - three-way comparison against values and errors", "[error][expected]")
{
	const mh::expected<int, long> hasValue(mh::expect, 5);
	const mh::expected<int, long> hasError(mh::unexpect, 3L);

	SECTION("expected <=> value")
	{
		REQUIRE(((hasValue <=> 5) == std::strong_ordering::equal));
		REQUIRE(((hasValue <=> 7) == std::strong_ordering::less));
		REQUIRE(((hasValue <=> 2) == std::strong_ordering::greater));
		// an expected holding an error compares less than any value
		REQUIRE(((hasError <=> 5) == std::strong_ordering::less));
	}
	SECTION("value <=> expected")
	{
		REQUIRE(((5 <=> hasValue) == std::strong_ordering::equal));
		REQUIRE(((7 <=> hasValue) == std::strong_ordering::greater));
		// any value compares greater than an expected holding an error
		REQUIRE(((5 <=> hasError) == std::strong_ordering::greater));
	}
	SECTION("expected <=> error")
	{
		REQUIRE(((hasError <=> 3L) == std::strong_ordering::equal));
		REQUIRE(((hasError <=> 4L) == std::strong_ordering::less));
		REQUIRE(((hasError <=> 1L) == std::strong_ordering::greater));
		// an expected holding a value compares less than any error
		REQUIRE(((hasValue <=> 3L) == std::strong_ordering::less));
	}
	SECTION("error <=> expected")
	{
		// this direction must mirror "expected <=> error" (it used to query the
		// wrong operand and could not even compile)
		REQUIRE(((3L <=> hasError) == std::strong_ordering::equal));
		REQUIRE(((4L <=> hasError) == std::strong_ordering::greater));
		REQUIRE(((1L <=> hasError) == std::strong_ordering::less));
		// any error compares greater than an expected holding a value
		REQUIRE(((3L <=> hasValue) == std::strong_ordering::greater));
	}
	SECTION("both directions are consistent")
	{
		REQUIRE(((3L <=> hasValue) == std::strong_ordering::greater));
		REQUIRE(((hasValue <=> 3L) == std::strong_ordering::less));
		REQUIRE(((3L <=> hasError) == std::strong_ordering::equal));
		REQUIRE(((hasError <=> 3L) == std::strong_ordering::equal));
	}
}
#endif

TEST_CASE("expected - equality against values and errors", "[error][expected]")
{
	const mh::expected<int, long> hasValue(mh::expect, 5);
	const mh::expected<int, long> hasError(mh::unexpect, 3L);

	REQUIRE(hasValue == 5);
	REQUIRE(5 == hasValue);
	REQUIRE(hasError == 3L);
	REQUIRE(3L == hasError);
	REQUIRE_FALSE(hasValue == 3L);
	REQUIRE_FALSE(3L == hasValue);
}

TEST_CASE("expected - converting assignment forwards its argument", "[error][expected]")
{
	mh::expected<int, std::shared_ptr<int>> exp(mh::expect, 1);
	REQUIRE(exp.has_value());

	// shared_ptr<int> is only constructible from an RVALUE unique_ptr, so this
	// assignment requires the converting operator= to actually forward
	std::unique_ptr<int> unique = std::make_unique<int>(5);
	exp = std::move(unique);

	REQUIRE(exp.has_error());
	REQUIRE(exp.error() != nullptr);
	REQUIRE(*exp.error() == 5);
	REQUIRE(unique == nullptr); // ownership was transferred, not duplicated
}

TEST_CASE("expected - assignment from an error lvalue copies", "[error][expected]")
{
	mh::expected<int, std::shared_ptr<int>> exp(mh::expect, 1);

	auto error = std::make_shared<int>(9);
	exp = error;

	REQUIRE(error != nullptr); // caller's object is untouched
	REQUIRE(exp.has_error());
	REQUIRE(exp.error().get() == error.get());
	REQUIRE(error.use_count() == 2);
}
