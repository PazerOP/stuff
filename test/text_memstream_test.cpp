#include "mh/text/memstream.hpp"
#include <catch2/catch.hpp>

#include <cstring>

TEST_CASE("memstream put", "[text][memstream]")
{
	char buf[128];
	mh::memstream ms(buf);

	constexpr std::string_view TEST_STRING = "my test string";
	ms << TEST_STRING;

	REQUIRE(std::memcmp(buf, TEST_STRING.data(), TEST_STRING.size()) == 0);
	REQUIRE(ms.view() == TEST_STRING);
	CHECK(!ms.fail());
	CHECK(ms.good());
	REQUIRE(ms.tellp() == 14);
	REQUIRE(ms.tellg() == 0);
	REQUIRE(ms.good());

	ms.seekp(7);
	ms << " foo";

	constexpr std::string_view TEST_STRING_FOO = "my test fooing";
	REQUIRE(ms.view() == TEST_STRING_FOO);
	REQUIRE(std::memcmp(buf, TEST_STRING_FOO.data(), TEST_STRING_FOO.size()) == 0);

	{
		REQUIRE(ms.seekg(2));
		std::string testWord;
		REQUIRE(ms >> testWord);
		REQUIRE(ms.good());
		REQUIRE(testWord == "test");

		REQUIRE(ms >> testWord);
		REQUIRE(ms.eof());
		REQUIRE(testWord == "fooing");

		ms.clear(ms.rdstate() & ~std::ios_base::eofbit);
		REQUIRE(ms.good());

		REQUIRE(ms.seekp(0));
		REQUIRE(ms.good());

		REQUIRE(ms.write("foo", 3));
		REQUIRE(ms.good());
		REQUIRE(ms.view_full() == "footest fooing");
		REQUIRE(ms.view() == "");
		REQUIRE(ms.seekg(1));
		REQUIRE(ms.view() == "ootest fooing");

		REQUIRE(ms.seekg(0));
		REQUIRE(ms.good());
		REQUIRE(ms.view() == "footest fooing");

		REQUIRE(ms << "bar");
		REQUIRE(ms.view() == "foobart fooing");
		REQUIRE(ms.good());
	}

	{
		constexpr int TEST_INT_VALUE = 487;

		REQUIRE(ms.view() == "foobart fooing");
		REQUIRE(ms.seekp(1, std::ios::beg));
		REQUIRE(ms.seekp(5, std::ios::cur));
		REQUIRE(ms.tellp() == 6);

		REQUIRE(ms.seekg(0, std::ios::end));
		REQUIRE(ms.tellg() == 14);
		REQUIRE(ms.seekg(0));

		ms << TEST_INT_VALUE;
		CHECK(ms.tellp() == 9);
		CHECK(ms.tellg() == 0);
		CHECK(ms.seekg(0, std::ios::end));
		CHECK(ms.tellg() == 14);
		CHECK(ms.seekg(0));

		CHECK(ms.view() == "foobar487ooing");
		CHECK(ms.view_full() == "foobar487ooing");

		int testInt;
		REQUIRE(ms.seekg(6));
		ms >> testInt;
		REQUIRE(testInt == TEST_INT_VALUE);
	}
}
