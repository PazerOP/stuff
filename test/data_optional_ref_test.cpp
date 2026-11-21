#include "mh/data/optional_ref.hpp"

#include <catch2/catch_all.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
	// the documented use case: an optional output parameter
	void write_answer(mh::optional_ref<int> out)
	{
		out = 42;
	}
}

TEST_CASE("optional_ref - type properties", "[data][optional_ref]")
{
	using ref = mh::optional_ref<int>;
	STATIC_CHECK(std::is_same_v<ref::value_type, int>);

	// reference semantics: copying/moving the wrapper itself is forbidden
	STATIC_CHECK(!std::is_copy_constructible_v<ref>);
	STATIC_CHECK(!std::is_move_constructible_v<ref>);
	STATIC_CHECK(!std::is_copy_assignable_v<ref>);
	STATIC_CHECK(!std::is_move_assignable_v<ref>);
}

TEST_CASE("optional_ref - bound to a value", "[data][optional_ref]")
{
	int target = 1;
	mh::optional_ref<int> ref(target);

	SECTION("get() aliases the target")
	{
		CHECK(&ref.get() == &target);

		ref.get() = 7;
		CHECK(target == 7);
	}
	SECTION("assignment writes through")
	{
		ref = 42;
		CHECK(target == 42);
	}
	SECTION("implicit conversion aliases the target")
	{
		int& alias = ref;
		CHECK(&alias == &target);

		const mh::optional_ref<int> constRef(target);
		const int& constAlias = constRef;
		CHECK(&constAlias == &target);
		CHECK(&constRef.get() == &target);
	}
}

TEST_CASE("optional_ref - unbound writes go to internal storage", "[data][optional_ref]")
{
	mh::optional_ref<int> ref;

	// the internal value is value-initialized
	CHECK(ref.get() == 0);

	// callers never need to null-check: writes land in the internal value
	ref = 42;
	CHECK(ref.get() == 42);

	ref.get() = 7;
	CHECK(ref.get() == 7);
}

TEST_CASE("optional_ref - move assignment moves into the target", "[data][optional_ref]")
{
	std::shared_ptr<int> target;
	mh::optional_ref<std::shared_ptr<int>> ref(target);

	auto source = std::make_shared<int>(9);
	ref = std::move(source);

	CHECK(source == nullptr); // moved from
	REQUIRE(target != nullptr);
	CHECK(*target == 9);

	// copy assignment leaves the source alone
	const auto other = std::make_shared<int>(10);
	ref = other;
	CHECK(other != nullptr);
	CHECK(target == other);
}

TEST_CASE("optional_ref - as an optional output parameter", "[data][optional_ref]")
{
	SECTION("caller wants the value")
	{
		int result = 0;
		write_answer(result);
		CHECK(result == 42);
	}
	SECTION("caller does not care")
	{
		write_answer({}); // must be well-defined despite no target
		SUCCEED("discarded output parameter is safe");
	}
}
