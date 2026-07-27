#include "mh/types/assert_cast.hpp"
#include "mh/types/disable_copy_move.hpp"
#include <catch2/catch_all.hpp>

#include <type_traits>

namespace
{
	struct copyable_not_movable : mh::disable_move
	{
		int m_Value = 5;
	};

	struct base
	{
		virtual ~base() = default;
		virtual int id() const { return 0; }
	};
	struct derived : base
	{
		int id() const override { return 1; }
		int m_Extra = 42;
	};
}

TEST_CASE("disable_copy - copy disabled, default construction fine", "[types][disable_copy_move]")
{
	STATIC_REQUIRE(std::is_default_constructible_v<mh::disable_copy>);
	STATIC_REQUIRE(!std::is_copy_constructible_v<mh::disable_copy>);
	STATIC_REQUIRE(!std::is_copy_assignable_v<mh::disable_copy>);
}

TEST_CASE("disable_move - move disabled but copy still allowed", "[types][disable_copy_move]")
{
	STATIC_REQUIRE(std::is_default_constructible_v<mh::disable_move>);
	STATIC_REQUIRE(std::is_copy_constructible_v<mh::disable_move>);
	STATIC_REQUIRE(std::is_copy_assignable_v<mh::disable_move>);
	STATIC_REQUIRE(!std::is_move_constructible_v<mh::disable_move>);
	STATIC_REQUIRE(!std::is_move_assignable_v<mh::disable_move>);
}

TEST_CASE("disable_move - derived types remain copyable", "[types][disable_copy_move]")
{
	copyable_not_movable original;
	original.m_Value = 7;

	copyable_not_movable copy(original);
	REQUIRE(copy.m_Value == 7);

	copy.m_Value = 9;
	original = copy;
	REQUIRE(original.m_Value == 9);

	// Note: for a DERIVED type the implicitly-defaulted move constructor is
	// defined as deleted and therefore ignored by overload resolution
	// ([class.copy.ctor]), so move syntax on the derived type compiles and
	// falls back to copying. Nothing can be stolen through the base.
	STATIC_REQUIRE(std::is_move_constructible_v<copyable_not_movable>);
	STATIC_REQUIRE(std::is_copy_constructible_v<copyable_not_movable>);
}

TEST_CASE("disable_copy_move - both copy and move disabled", "[types][disable_copy_move]")
{
	STATIC_REQUIRE(std::is_default_constructible_v<mh::disable_copy_move>);
	STATIC_REQUIRE(!std::is_copy_constructible_v<mh::disable_copy_move>);
	STATIC_REQUIRE(!std::is_copy_assignable_v<mh::disable_copy_move>);
	STATIC_REQUIRE(!std::is_move_constructible_v<mh::disable_copy_move>);
	STATIC_REQUIRE(!std::is_move_assignable_v<mh::disable_copy_move>);
}

TEST_CASE("assert_cast - reference downcast preserves object identity", "[types][assert_cast]")
{
	derived object;
	base& baseRef = object;

	// the returned reference must refer to the ORIGINAL object (no copy, no
	// slicing, no dangling reference to a function-local)
	derived& downcast = mh::assert_cast<derived&>(baseRef);
	REQUIRE(&downcast == &object);
	REQUIRE(downcast.id() == 1);
	REQUIRE(downcast.m_Extra == 42);

	downcast.m_Extra = 7;
	REQUIRE(object.m_Extra == 7);
}

TEST_CASE("assert_cast - const reference downcast preserves object identity", "[types][assert_cast]")
{
	derived object;
	const base& baseRef = object;

	const derived& downcast = mh::assert_cast<const derived&>(baseRef);
	REQUIRE(&downcast == &object);
	REQUIRE(downcast.id() == 1);
}

TEST_CASE("assert_cast - pointer downcast", "[types][assert_cast]")
{
	derived object;
	base* basePtr = &object;

	derived* downcast = mh::assert_cast<derived*>(basePtr);
	REQUIRE(downcast == &object);
	REQUIRE(downcast->m_Extra == 42);
}

TEST_CASE("assert_cast - value conversions still compile", "[types][assert_cast]")
{
	REQUIRE(mh::assert_cast<int>(2.0) == 2);

	const long value = 17;
	REQUIRE(mh::assert_cast<int>(value) == 17);
}
