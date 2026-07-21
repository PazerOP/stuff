#include "mh/math/angles.hpp"
#include <catch2/catch_all.hpp>

// The conversion factors are derived from std::numbers::pi in double precision.
static_assert(mh::deg2rad(1.0) == std::numbers::pi / 180.0);
static_assert(mh::rad2deg(1.0) == 180.0 / std::numbers::pi);

TEST_CASE("deg2rad/rad2deg", "[math][angles]")
{
	CHECK(mh::deg2rad(180.0) == Catch::Approx(std::numbers::pi));
	CHECK(mh::rad2deg(std::numbers::pi) == Catch::Approx(180.0));
	CHECK(mh::rad2deg(mh::deg2rad(90.0f)) == Catch::Approx(90.0f));
}
