#include "mh/io/file.hpp"

#include <catch2/catch_all.hpp>

#include <filesystem>
#include <locale>
#include <string>

namespace
{
	struct global_locale_restorer
	{
		std::locale previous = std::locale();
		~global_locale_restorer() { std::locale::global(previous); }
	};

	std::filesystem::path temp_file(const char* name)
	{
		return std::filesystem::temp_directory_path() / name;
	}
}

TEST_CASE("read_file/write_file - byte round trip", "[io][file]")
{
	const auto path = temp_file("mh_stuff_io_file_test.txt");
	const std::string contents = "line one\nline two\n";

	mh::write_file(path, std::string_view(contents));
	CHECK(mh::read_file(path) == contents);

	std::filesystem::remove(path);
}

TEST_CASE("read_file - empty file", "[io][file]")
{
	const auto path = temp_file("mh_stuff_io_file_empty.txt");
	mh::write_file(path, std::string_view{});

	CHECK(mh::read_file(path).empty());

	std::filesystem::remove(path);
}

TEST_CASE("read_file - reads that extract fewer chars than bytes are not errors", "[io][file]")
{
	// regression: read_file sized the string from tellg() (external BYTE length),
	// enabled failbit exceptions, then read() exactly that many CHARACTERS.
	// Whenever the stream shrinks the data - CRLF translation on Windows, or any
	// multi-byte codecvt for wide streams - read() hit EOF short and THREW on a
	// perfectly valid file. A UTF-8 global locale + wide read reproduces this
	// portably on Linux: 8 bytes -> 6 wide characters.
	std::locale utf8_locale;
	try
	{
		utf8_locale = std::locale("C.UTF-8");
	}
	catch (const std::exception&)
	{
		SKIP("C.UTF-8 locale not available on this system");
	}

	const auto path = temp_file("mh_stuff_io_file_utf8.txt");
	mh::write_file(path, std::string_view("h\xC3\xA9llo\xC2\xA1")); // "hello" with e-acute + inverted-! in UTF-8

	global_locale_restorer restore;
	std::locale::global(utf8_locale);

	std::wstring wide;
	REQUIRE_NOTHROW(wide = mh::read_file<wchar_t>(path));
	CHECK(wide.size() == 6);
	CHECK(wide == L"h\u00E9llo\u00A1");

	std::filesystem::remove(path);
}

TEST_CASE("read_file - missing file still throws", "[io][file]")
{
	CHECK_THROWS(mh::read_file(temp_file("mh_stuff_io_file_does_not_exist.txt")));
}
