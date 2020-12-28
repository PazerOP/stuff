#include "mh/text/memstream.hpp"
#include "catch2/repo/single_include/catch2/catch.hpp"

TEST_CASE("memstream put", "[text][memstream]")
{
	char buf[128];
	mh::memstream ms(buf);

	constexpr std::string_view TEST_STRING = "my test string";
	ms << TEST_STRING;

	REQUIRE_THAT(buf, Catch::Equals(TEST_STRING.data()));
	REQUIRE(ms.view() == TEST_STRING);
	CHECK(!ms.fail());
	CHECK(ms.good());
	REQUIRE(ms.tellp() == 14);
	REQUIRE(ms.tellg() == 0);
	REQUIRE(ms.good());

	ms.seekp(7);
	ms << " foo";
	REQUIRE(ms.view() == "my test foo");
	REQUIRE_THAT(buf, Catch::Equals("my test foo"));

	{
		REQUIRE(ms.seekg(2));
		std::string testWord;
		REQUIRE(ms >> testWord);
		REQUIRE(ms.good());
		REQUIRE(testWord == "test");

		REQUIRE(ms >> testWord);
		REQUIRE(ms.eof());
		REQUIRE(testWord == "foo");

		ms.clear(ms.rdstate() & ~std::ios_base::eofbit);
		REQUIRE(ms.good());

		REQUIRE(ms.seekp(0));
		REQUIRE(ms.good());

		REQUIRE(ms.write("foo", 3));
		REQUIRE(ms.good());
		REQUIRE(ms.view_full() == "foo");
		REQUIRE(ms.view() == "");
		REQUIRE(ms.seekg(1));
		REQUIRE(ms.view() == "oo");

		REQUIRE(ms.seekg(0));
		REQUIRE(ms.good());
		REQUIRE(ms.view() == "foo");

		REQUIRE(ms << "bar");
		REQUIRE(ms.view() == "foobar");
		REQUIRE(ms.good());
	}

	{
		constexpr int TEST_INT_VALUE = 487;

		REQUIRE(ms.view() == "foobar");
		REQUIRE(ms.seekp(0));
		REQUIRE(ms.tellp() == 0);

		REQUIRE(ms.seekg(0, std::ios::end));
		REQUIRE(ms.tellg() == 6);
		REQUIRE(ms.seekg(0, std::ios::beg));

		ms << TEST_INT_VALUE;
		REQUIRE(ms.view() == "487bar");

		int testInt;
		REQUIRE(ms.seekg(0));
		ms >> testInt;
		REQUIRE(testInt == TEST_INT_VALUE);
	}
}
