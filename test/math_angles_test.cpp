#include "mh/math/angles.hpp"

#include <catch2/catch_all.hpp>

#include <numbers>
#include <type_traits>

TEST_CASE("deg2rad / rad2deg", "[math][angles]")
{
	CHECK(mh::deg2rad(180.0) == Catch::Approx(std::numbers::pi));
	CHECK(mh::deg2rad(90.0f) == Catch::Approx(std::numbers::pi / 2));
	CHECK(mh::deg2rad(0.0) == 0.0);
	CHECK(mh::deg2rad(-180.0) == Catch::Approx(-std::numbers::pi));

	CHECK(mh::rad2deg(std::numbers::pi) == Catch::Approx(180.0));
	CHECK(mh::rad2deg(0.0f) == 0.0f);
	CHECK(mh::rad2deg(-std::numbers::pi / 2) == Catch::Approx(-90.0));

	// round trip
	CHECK(mh::rad2deg(mh::deg2rad(123.5)) == Catch::Approx(123.5));

	// integral input promotes to (at least) float; floating input keeps its type
	STATIC_CHECK(std::is_same_v<decltype(mh::deg2rad(180)), float>);
	STATIC_CHECK(std::is_same_v<decltype(mh::deg2rad(180.0)), double>);
	STATIC_CHECK(std::is_same_v<decltype(mh::rad2deg(1.0f)), float>);
	CHECK(mh::deg2rad(180) == Catch::Approx(std::numbers::pi));

	// usable in constant expressions
	STATIC_CHECK(mh::deg2rad(0) == 0.0f);
	STATIC_CHECK(mh::rad2deg(0.0) == 0.0);
}
