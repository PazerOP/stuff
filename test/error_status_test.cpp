#include "mh/error/status.hpp"
#include <catch2/catch_all.hpp>

#include <string>

namespace
{
	// ok deliberately equals the value of a default-constructed test_status
	enum class test_status
	{
		ok = 0,
		failed = 1,
	};
}

TEST_CASE("status - freshly constructed source has no value", "[error][status]")
{
	mh::status_source<test_status> source;
	auto reader = source.get_reader();
	REQUIRE(!reader.has_value());
}

TEST_CASE("status - setting a value equal to the default state still counts", "[error][status]")
{
	mh::status_source<test_status> source;
	auto reader = source.get_reader();

	// test_status::ok compares equal to the default-constructed status, but an
	// explicit set() must still flip has_value()
	const bool changed = source.set(test_status::ok, "");
	REQUIRE(changed);
	REQUIRE(reader.has_value());
	REQUIRE(reader.get().m_Status == test_status::ok);
	REQUIRE(reader.get().m_Message == "");

	// setting the identical value again is not a change, but the value remains
	REQUIRE(!source.set(test_status::ok, ""));
	REQUIRE(reader.has_value());
}

TEST_CASE("status - set() reports changes of code and message", "[error][status]")
{
	mh::status_source<test_status> source;
	auto reader = source.get_reader();

	REQUIRE(source.set(test_status::failed, "something broke"));
	REQUIRE(reader.has_value());
	REQUIRE(reader.get().m_Status == test_status::failed);
	REQUIRE(reader.get().m_Message == "something broke");

	// same code, new message -> still a change
	REQUIRE(source.set(test_status::failed, "different message"));
	REQUIRE(reader.get().m_Message == "different message");

	// identical code and message -> no change
	REQUIRE(!source.set(test_status::failed, "different message"));
}
