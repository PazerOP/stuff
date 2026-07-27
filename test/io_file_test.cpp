// The header must be usable standalone: it has to provide its own MH_STUFF_API
// fallback even when the build system's definition is not in effect.
#ifdef MH_STUFF_API
#undef MH_STUFF_API
#endif
#include "mh/io/file.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <locale>
#include <string>
#include "last_include.hpp"

using namespace std::string_literals;

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

TEST_CASE("read_file with char", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_read_char.txt";
    const std::string test_content = "Hello, World!\nThis is a test file.\n";
    
    // Create test file
    {
        std::ofstream file(test_file);
        file << test_content;
    }
    
    SECTION("read_file returns correct content")
    {
        auto result = mh::read_file(test_file);
        REQUIRE(result == test_content);
    }
    
    SECTION("read_file with explicit template parameters")
    {
        auto result = mh::read_file<char>(test_file);
        REQUIRE(result == test_content);
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("read_file with wchar_t", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_read_wchar.txt";
    const std::wstring test_content = L"Hello, World!\nThis is a wide test file.\n";
    
    // Create test file
    {
        std::wofstream file(test_file);
        file << test_content;
    }
    
    SECTION("read_file returns correct wide content")
    {
        auto result = mh::read_file<wchar_t>(test_file);
        REQUIRE(result == test_content);
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("write_file with string_view", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_write_string_view.txt";
    const std::string test_content = "Written by write_file!\nMultiple lines here.\n";
    
    SECTION("write_file creates file with correct content")
    {
        mh::write_file(test_file, std::string_view(test_content));
        
        // Verify by reading back
        std::ifstream file(test_file);
        std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        REQUIRE(result == test_content);
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("write_file with C-style string", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_write_cstring.txt";
    const char* test_content = "C-style string content";
    
    SECTION("write_file handles C-style strings")
    {
        mh::write_file(test_file, test_content);
        
        // Verify by reading back
        auto result = mh::read_file(test_file);
        REQUIRE(result == std::string(test_content));
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("write_file with wide characters", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_write_wide.txt";
    const std::wstring test_content = L"Wide character content\nSecond line";
    
    SECTION("write_file handles wide characters")
    {
        mh::write_file(test_file, std::wstring_view(test_content));
        
        // Verify by reading back
        auto result = mh::read_file<wchar_t>(test_file);
        REQUIRE(result == test_content);
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("file operations error handling", "[io][file]")
{
    const auto nonexistent_file = std::filesystem::temp_directory_path() / "nonexistent_directory_12345" / "file.txt";
    
    SECTION("read_file throws on nonexistent file")
    {
        REQUIRE_THROWS_AS(mh::read_file(nonexistent_file), std::ios_base::failure);
    }
    
    SECTION("write_file throws on invalid path")
    {
        REQUIRE_THROWS_AS(mh::write_file(nonexistent_file, "content"), std::ios_base::failure);
    }
}

TEST_CASE("round-trip file operations", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_roundtrip.txt";
    
    SECTION("char round-trip")
    {
        const std::string original = "Round-trip test\nWith multiple lines\nAnd special chars: @#$%^&*()";
        
        mh::write_file(test_file, std::string_view(original));
        auto result = mh::read_file(test_file);
        
        REQUIRE(result == original);
    }
    
    SECTION("wchar_t round-trip")
    {
        const std::wstring original = L"Wide round-trip\nSecond line";
        
        mh::write_file(test_file, std::wstring_view(original));
        auto result = mh::read_file<wchar_t>(test_file);
        
        REQUIRE(result == original);
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("empty file operations", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_empty.txt";
    
    SECTION("empty file read/write")
    {
        const std::string empty_content = "";
        
        mh::write_file(test_file, std::string_view(empty_content));
        auto result = mh::read_file(test_file);
        
        REQUIRE(result == empty_content);
        REQUIRE(result.empty());
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("large file operations", "[io][file]")
{
    const auto test_file = std::filesystem::temp_directory_path() / "mh_test_large.txt";
    
    SECTION("large content handling")
    {
        // Create a large string (>100KB)
        std::string large_content;
        large_content.reserve(200000);
        for (int i = 0; i < 2500; ++i) {
            large_content += "This is line " + std::to_string(i) + " of the large test file content.\n";
        }
        
        mh::write_file(test_file, std::string_view(large_content));
        auto result = mh::read_file(test_file);
        
        REQUIRE(result == large_content);
        REQUIRE(result.size() > 100000); // Verify it's actually large
    }
    
    // Cleanup
    std::filesystem::remove(test_file);
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
