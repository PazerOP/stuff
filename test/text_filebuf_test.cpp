#include "mh/text/filebuf.hpp"
#include <catch2/catch_all.hpp>

#include <cstdio>
#include <cstring>
#include <ostream>

// fmemopen is POSIX-only, not available on Windows
#ifdef __unix__

TEST_CASE("filebuf basic write", "[text][filebuf]")
{
	char buf[128] = {};
	FILE* f = fmemopen(buf, sizeof(buf), "w");
	REQUIRE(f != nullptr);

	{
		mh::filebuf fb(f);
		std::ostream os(&fb);

		os << "Hello, world!";
		os.flush();
	}

	fclose(f);

	REQUIRE(std::strcmp(buf, "Hello, world!") == 0);
}

TEST_CASE("filebuf multiline write", "[text][filebuf]")
{
	char buf[256] = {};
	FILE* f = fmemopen(buf, sizeof(buf), "w");
	REQUIRE(f != nullptr);

	{
		mh::filebuf fb(f);
		std::ostream os(&fb);

		os << "Line 1\n";
		os << "Line 2\n";
		os << "Line 3";
		os.flush();
	}

	fclose(f);

	REQUIRE(std::strcmp(buf, "Line 1\nLine 2\nLine 3") == 0);
}

TEST_CASE("filebuf large write", "[text][filebuf]")
{
	char buf[1024] = {};
	FILE* f = fmemopen(buf, sizeof(buf), "w");
	REQUIRE(f != nullptr);

	{
		mh::filebuf fb(f);
		std::ostream os(&fb);

		// Write a large string to test xsputn
		std::string large_str(512, 'A');
		os << large_str;
		os.flush();
	}

	fclose(f);

	REQUIRE(std::strlen(buf) == 512);
	REQUIRE(buf[0] == 'A');
	REQUIRE(buf[511] == 'A');
}

TEST_CASE("filebuf overflow", "[text][filebuf]")
{
	char buf[16] = {};
	FILE* f = fmemopen(buf, sizeof(buf), "w");
	REQUIRE(f != nullptr);

	{
		mh::filebuf fb(f);

		// Test overflow method by writing individual characters
		for (char c = 'A'; c <= 'J'; ++c)
		{
			fb.sputc(c);
		}
		fb.pubsync();
	}

	fclose(f);

	REQUIRE(std::strcmp(buf, "ABCDEFGHIJ") == 0);
}

TEST_CASE("filebuf with format", "[text][filebuf]")
{
	char buf[256] = {};
	FILE* f = fmemopen(buf, sizeof(buf), "w");
	REQUIRE(f != nullptr);

	{
		mh::filebuf fb(f);
		std::ostream os(&fb);

		os << "Integer: " << 42 << '\n';
		os << "Float: " << 3.14 << '\n';
		os << "String: " << "test";
		os.flush();
	}

	fclose(f);

	REQUIRE(std::strstr(buf, "Integer: 42") != nullptr);
	REQUIRE(std::strstr(buf, "Float: 3.14") != nullptr);
	REQUIRE(std::strstr(buf, "String: test") != nullptr);
}

#endif // __unix__
