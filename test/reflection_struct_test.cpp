#include "mh/reflection/struct.hpp"

#if (__cpp_concepts >= 201907) || (_MSC_VER >= 1928)

#include <catch2/catch_all.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace
{
	struct Point
	{
		int x = 1;
		int y = 2;
	};

	struct Base
	{
		int base_value = 10;
	};

	struct Derived : Base
	{
		MH_STRUCT_REFLECT_BASES(Base)
		int derived_value = 20;
	};
}

MH_STRUCT_REFLECT_BEGIN(Point)
	MH_STRUCT_REFLECT_MEMBER(x)
	MH_STRUCT_REFLECT_MEMBER(y)
MH_STRUCT_REFLECT_END()

MH_STRUCT_REFLECT_BEGIN(Base)
	MH_STRUCT_REFLECT_MEMBER(base_value)
MH_STRUCT_REFLECT_END()

// offsetof on a type with bases is conditionally-supported; both GCC and clang
// compute it correctly for single inheritance but diagnose under -Werror
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
MH_STRUCT_REFLECT_BEGIN(Derived)
	MH_STRUCT_REFLECT_MEMBER(derived_value)
MH_STRUCT_REFLECT_END()
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

TEST_CASE("struct reflection - for_each_member on an lvalue", "[reflection][struct]")
{
	// regression: the generated for_each forwarded the object as
	// std::forward<value_type> (an rvalue cast), so for_each_member on an LVALUE
	// failed to compile for every reflected struct - only rvalues worked.
	Point p;
	std::vector<std::string> names;
	mh::for_each_member(p, [&](const auto& member)
	{
		names.emplace_back(member.name);
	});

	REQUIRE(names == std::vector<std::string>{ "x", "y" });
}

TEST_CASE("struct reflection - members are mutable through struct_member_info", "[reflection][struct]")
{
	Point p;
	mh::for_each_member(p, [&](const auto& member)
	{
		member.value += 100;
	});

	CHECK(p.x == 101);
	CHECK(p.y == 102);
}

TEST_CASE("struct reflection - rvalues still work", "[reflection][struct]")
{
	int sum = 0;
	mh::for_each_member(Point{}, [&](const auto& member)
	{
		sum += member.value;
	});

	CHECK(sum == 3);
}

TEST_CASE("struct reflection - declared base types are traversed", "[reflection][struct]")
{
	// regression: HasBaseTypes was checked on the REFERENCE type
	// (T&::mh_struct_reflect_bases_t never exists), so bases were never visited.
	Derived d;
	std::vector<std::string> names;
	mh::for_each_member(d, [&](const auto& member)
	{
		names.emplace_back(member.name);
	});

	REQUIRE(names == std::vector<std::string>{ "base_value", "derived_value" });

	// base members must alias the same object
	mh::for_each_member(d, [&](const auto& member)
	{
		member.value *= 2;
	});
	CHECK(d.base_value == 20);
	CHECK(d.derived_value == 40);
}

TEST_CASE("struct reflection - member metadata", "[reflection][struct]")
{
	Point p;
	mh::for_each_member(p, [&](const auto& member)
	{
		if (member.name == std::string_view("x"))
		{
			CHECK(member.offset == offsetof(Point, x));
			CHECK(&(member.obj.*member.pointer) == &p.x);
			CHECK(&member.value == &p.x);
		}
	});
}

#endif
