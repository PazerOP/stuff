#include <catch2/catch_test_macros.hpp>
#include <mh/io/filesystem_helpers.hpp>
#include <filesystem>
#include "last_include.hpp"

TEST_CASE("filename_without_extension basic functionality", "[io][filesystem_helpers]")
{
    SECTION("simple filename with extension")
    {
        std::filesystem::path input = "test.txt";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "test");
    }
    
    SECTION("filename with multiple dots")
    {
        std::filesystem::path input = "archive.tar.gz";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "archive.tar");
    }
    
    SECTION("filename without extension")
    {
        std::filesystem::path input = "README";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "README");
    }
    
    SECTION("full path with extension")
    {
        std::filesystem::path input = "/path/to/file.cpp";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "file");
    }
    
    SECTION("hidden file with extension")
    {
        std::filesystem::path input = ".gitignore";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == ".gitignore");
    }
    
    SECTION("hidden file with explicit extension")
    {
        std::filesystem::path input = ".config.json";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == ".config");
    }
}

TEST_CASE("filename_without_extension edge cases", "[io][filesystem_helpers]")
{
    SECTION("empty path")
    {
        std::filesystem::path input = "";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "");
    }
    
    SECTION("just extension")
    {
        std::filesystem::path input = ".txt";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == ".txt");
    }
    
    SECTION("directory only")
    {
        std::filesystem::path input = "/path/to/directory/";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "");
    }
    
    SECTION("filename ending with dot")
    {
        std::filesystem::path input = "file.";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.string() == "file");
    }
}

TEST_CASE("replace_filename_keep_extension basic functionality", "[io][filesystem_helpers]")
{
    SECTION("simple replacement")
    {
        std::filesystem::path input = "old_file.txt";
        std::filesystem::path new_filename = "new_file";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "new_file.txt");
    }
    
    SECTION("full path replacement")
    {
        std::filesystem::path input = "/path/to/old_file.cpp";
        std::filesystem::path new_filename = "new_file";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "/path/to/new_file.cpp");
    }
    
    SECTION("multiple extension replacement")
    {
        std::filesystem::path input = "archive.tar.gz";
        std::filesystem::path new_filename = "backup";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "backup.gz");
    }
    
    SECTION("no extension in original")
    {
        std::filesystem::path input = "/path/to/README";
        std::filesystem::path new_filename = "CHANGELOG";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "/path/to/CHANGELOG");
    }
}

TEST_CASE("replace_filename_keep_extension with new filename having extension", "[io][filesystem_helpers]")
{
    SECTION("new filename has extension that gets replaced")
    {
        std::filesystem::path input = "old_file.txt";
        std::filesystem::path new_filename = "new_file.hpp";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "new_file.txt");
    }
    
    SECTION("complex path with extension override")
    {
        std::filesystem::path input = "/complex/path/file.cpp";
        std::filesystem::path new_filename = "replacement.py";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "/complex/path/replacement.cpp");
    }
    
    SECTION("multiple extensions in both paths")
    {
        std::filesystem::path input = "backup.tar.gz";
        std::filesystem::path new_filename = "archive.zip.bak";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "archive.zip.gz");
    }
}

TEST_CASE("replace_filename_keep_extension edge cases", "[io][filesystem_helpers]")
{
    SECTION("empty new filename")
    {
        std::filesystem::path input = "file.txt";
        std::filesystem::path new_filename = "";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == ".txt");
    }
    
    SECTION("new filename with just extension")
    {
        std::filesystem::path input = "file.txt";
        std::filesystem::path new_filename = ".cpp";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == ".cpp.txt");
    }
    
    SECTION("hidden file original")
    {
        std::filesystem::path input = ".gitignore";
        std::filesystem::path new_filename = "ignore";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "ignore");
    }
    
    SECTION("directory path handling")
    {
        std::filesystem::path input = "/path/to/directory/";
        std::filesystem::path new_filename = "file";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.string() == "/path/to/directory/file");
    }
}

TEST_CASE("filesystem helpers with Unicode paths", "[io][filesystem_helpers]")
{
    SECTION("Unicode filename without extension")
    {
        std::filesystem::path input = u8"тест.txt";
        auto result = mh::filename_without_extension(input);
        REQUIRE(result.u8string() == u8"тест");
    }
    
    SECTION("Unicode path replacement")
    {
        std::filesystem::path input = u8"/путь/файл.cpp";
        std::filesystem::path new_filename = u8"новый";
        auto result = mh::replace_filename_keep_extension(input, new_filename);
        REQUIRE(result.u8string() == u8"/путь/новый.cpp");
    }
}

TEST_CASE("filesystem helpers move semantics", "[io][filesystem_helpers]")
{
    SECTION("filename_without_extension preserves move")
    {
        std::filesystem::path input = "/very/long/path/to/some/deeply/nested/file.extension";
        const auto input_copy = input;
        
        auto result = mh::filename_without_extension(std::move(input));
        REQUIRE(result.string() == "file");
        
        // Original should still be valid (move doesn't guarantee emptying)
        // but we shouldn't rely on its state
    }
    
    SECTION("replace_filename_keep_extension preserves move")
    {
        std::filesystem::path input = "/very/long/path/to/some/deeply/nested/file.cpp";
        std::filesystem::path new_filename = "replacement";
        const auto expected = "/very/long/path/to/some/deeply/nested/replacement.cpp";
        
        auto result = mh::replace_filename_keep_extension(std::move(input), std::move(new_filename));
        REQUIRE(result.string() == expected);
    }
}