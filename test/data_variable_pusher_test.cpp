#include "mh/data/variable_pusher.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

TEST_CASE("variable_pusher trivial")
{
	int var = 10;
	REQUIRE(var == 10);

	{
		mh::variable_pusher pusher(var, 46);
		REQUIRE(var == 46);
		REQUIRE(pusher.old_value() == 10);

		{
			mh::variable_pusher pusher2(var, 42);
			REQUIRE(var == 42);
			REQUIRE(pusher2.old_value() == 46);
			REQUIRE(pusher.old_value() == 10);
		}

		REQUIRE(var == 46);
		REQUIRE(pusher.old_value() == 10);
	}

	REQUIRE(var == 10);
}

TEST_CASE("variable_pusher complex")
{
	std::string var = "hello";
	REQUIRE(var == "hello");

	{
		std::string newValue("new_value");
		mh::variable_pusher pusher(var, newValue);
		REQUIRE(var == "new_value");
		REQUIRE(pusher.old_value() == "hello");
		REQUIRE(newValue == "new_value");
		{
			std::string newValue2("another new value!");
			mh::variable_pusher pusher2(var, std::move(newValue2));
			REQUIRE(newValue2.empty());
			REQUIRE(var == "another new value!");
			REQUIRE(pusher2.old_value() == "new_value");
			REQUIRE(pusher.old_value() == "hello");
		}

		REQUIRE(var == "new_value");
		REQUIRE(pusher.old_value() == "hello");
		REQUIRE(newValue == "new_value");
	}

	REQUIRE(var == "hello");
}
