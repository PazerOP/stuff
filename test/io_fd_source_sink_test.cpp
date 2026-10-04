#include <catch2/catch_test_macros.hpp>
#include <mh/io/fd_source.hpp>
#include <mh/io/fd_sink.hpp>
#include <mh/coroutine/task.hpp>

#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include "last_include.hpp"

TEST_CASE("fd_source file operations", "[io][fd_source]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_fd_source.txt";
    const std::string test_content = "Hello, fd_source!\nThis is test data for reading.\n";

    // Create test file
    {
        std::ofstream file(temp_file);
        file << test_content;
    }

    SECTION("create_file opens file for reading")
    {
        auto source = mh::io::source::create_file(temp_file);
        REQUIRE(source != nullptr);
        REQUIRE(source->is_open());
        REQUIRE(source->get_native_handle() >= 0);
    }

    SECTION("read_async reads file content")
    {
        auto source = mh::io::source::create_file(temp_file);

        char buffer[1024] = {0};
        auto task = source->read_async(buffer, sizeof(buffer) - 1);
        auto bytes_read = task.get(); // Blocking get for test

        REQUIRE(bytes_read == test_content.size());
        REQUIRE(std::string(buffer, bytes_read) == test_content);
    }

    SECTION("read_async with smaller buffer reads partial content")
    {
        auto source = mh::io::source::create_file(temp_file);

        char buffer[10] = {0};
        auto task = source->read_async(buffer, sizeof(buffer) - 1);
        auto bytes_read = task.get();

        REQUIRE(bytes_read == 9); // sizeof(buffer) - 1
        REQUIRE(std::string(buffer, bytes_read) == test_content.substr(0, 9));
    }

    SECTION("close makes source unusable")
    {
        auto source = mh::io::source::create_file(temp_file);
        REQUIRE(source->is_open());

        source->close();
        REQUIRE(!source->is_open());

        char buffer[10];
        REQUIRE_THROWS_AS(source->read_async(buffer, sizeof(buffer)).get(), std::runtime_error);
    }

    SECTION("create_file throws on nonexistent file")
    {
        auto nonexistent_file = std::filesystem::temp_directory_path() / "nonexistent_12345.txt";
        REQUIRE_THROWS_AS(mh::io::source::create_file(nonexistent_file), std::runtime_error);
    }

    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("fd_sink file operations", "[io][fd_sink]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_fd_sink.txt";
    const std::string test_content = "Hello, fd_sink!\nThis is test data for writing.\n";

    SECTION("create_file opens file for writing")
    {
        auto sink = mh::io::sink::create_file(temp_file);
        REQUIRE(sink != nullptr);
        REQUIRE(sink->is_open());
        REQUIRE(sink->get_native_handle() >= 0);

        // Cleanup
        std::filesystem::remove(temp_file);
    }

    SECTION("write_async writes content to file")
    {
        auto sink = mh::io::sink::create_file(temp_file);

        auto task = sink->write_async(test_content.data(), test_content.size());
        auto bytes_written = task.get(); // Blocking get for test

        REQUIRE(bytes_written == test_content.size());

        sink->close();

        // Verify by reading back
        std::ifstream file(temp_file);
        std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        REQUIRE(result == test_content);

        // Cleanup
        std::filesystem::remove(temp_file);
    }

    SECTION("create_file with append mode")
    {
        const std::string first_content = "First line\n";
        const std::string second_content = "Second line\n";

        // Write first content
        {
            auto sink = mh::io::sink::create_file(temp_file, false); // Don't append
            sink->write_async(first_content.data(), first_content.size()).get();
        }

        // Append second content
        {
            auto sink = mh::io::sink::create_file(temp_file, true); // Append
            sink->write_async(second_content.data(), second_content.size()).get();
        }

        // Verify both contents
        std::ifstream file(temp_file);
        std::string result((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        REQUIRE(result == first_content + second_content);

        // Cleanup
        std::filesystem::remove(temp_file);
    }

    SECTION("close makes sink unusable")
    {
        auto sink = mh::io::sink::create_file(temp_file);
        REQUIRE(sink->is_open());

        sink->close();
        REQUIRE(!sink->is_open());

        REQUIRE_THROWS_AS(sink->write_async(test_content.data(), test_content.size()).get(), std::runtime_error);

        // Cleanup
        std::filesystem::remove(temp_file);
    }
}

TEST_CASE("fd_source constructor behavior", "[io][fd_source]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_fd_constructor.txt";

    // Create test file
    {
        std::ofstream file(temp_file);
        file << "test content";
    }

    SECTION("constructor with take_ownership=true")
    {
        int fd = open(temp_file.c_str(), O_RDONLY);
        REQUIRE(fd >= 0);

        {
            mh::io::fd_source source(fd, true);
            REQUIRE(source.is_open());
            REQUIRE(source.get_native_handle() == fd);
        }

        // File descriptor should be closed now
        char buffer[1];
        ssize_t result = read(fd, buffer, 1);
        REQUIRE(result == -1); // Should fail because fd is closed
    }

    SECTION("constructor with take_ownership=false")
    {
        int fd = open(temp_file.c_str(), O_RDONLY);
        REQUIRE(fd >= 0);

        {
            // take_ownership=false calls dup(), so source gets its own fd
            mh::io::fd_source source(fd, false);
            REQUIRE(source.is_open());
            // The source should have a different fd due to dup()
            REQUIRE(source.get_native_handle() != fd);
        }

        // Original fd should still be open
        char buffer[1];
        ssize_t result = read(fd, buffer, 1);
        REQUIRE(result >= 0); // Should succeed because original fd is still open

        close(fd); // Clean up original fd
    }

    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("fd_sink constructor behavior", "[io][fd_sink]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_fd_sink_constructor.txt";

    SECTION("constructor with take_ownership=true")
    {
        int fd = open(temp_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        REQUIRE(fd >= 0);

        {
            mh::io::fd_sink sink(fd, true);
            REQUIRE(sink.is_open());
            REQUIRE(sink.get_native_handle() == fd);
        }

        // File descriptor should be closed now
        const char* test_data = "test";
        ssize_t result = write(fd, test_data, strlen(test_data));
        REQUIRE(result == -1); // Should fail because fd is closed
    }

    SECTION("constructor with take_ownership=false")
    {
        int fd = open(temp_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        REQUIRE(fd >= 0);

        {
            // take_ownership=false calls dup(), so sink gets its own fd
            mh::io::fd_sink sink(fd, false);
            REQUIRE(sink.is_open());
            // The sink should have a different fd due to dup()
            REQUIRE(sink.get_native_handle() != fd);
        }

        // Original fd should still be open
        const char* test_data = "test";
        ssize_t result = write(fd, test_data, strlen(test_data));
        REQUIRE(result >= 0); // Should succeed because original fd is still open

        close(fd); // Clean up original fd
    }

    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("standard stream singleton prevention", "[io][fd_source][fd_sink]")
{
    SECTION("multiple stdout fd_source creation should throw")
    {
        // First creation should succeed (done by stdout_source() singleton)
        auto stdout_src = mh::io::source::stdout_source();
        REQUIRE(stdout_src != nullptr);

        // Attempting to create another should throw
        REQUIRE_THROWS_AS(mh::io::fd_source(STDOUT_FILENO, false), std::runtime_error);
    }

    SECTION("multiple stdin fd_sink creation should throw")
    {
        // First creation should succeed (done by stdin_sink() singleton)
        auto stdin_snk = mh::io::sink::stdin_sink();
        REQUIRE(stdin_snk != nullptr);

        // Attempting to create another should throw
        REQUIRE_THROWS_AS(mh::io::fd_sink(STDIN_FILENO, false), std::runtime_error);
    }
}

TEST_CASE("fd_source and fd_sink round-trip", "[io][fd_source][fd_sink]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_roundtrip.txt";
    const std::string test_content = "Round-trip test data\nWith multiple lines\nAnd special characters: !@#$%^&*()\n";

    SECTION("write then read produces same content")
    {
        // Write content using fd_sink
        {
            auto sink = mh::io::sink::create_file(temp_file);
            auto bytes_written = sink->write_async(test_content.data(), test_content.size()).get();
            REQUIRE(bytes_written == test_content.size());
        }

        // Read content using fd_source
        {
            auto source = mh::io::source::create_file(temp_file);
            char buffer[1024] = {0};
            auto bytes_read = source->read_async(buffer, sizeof(buffer) - 1).get();

            REQUIRE(bytes_read == test_content.size());
            REQUIRE(std::string(buffer, bytes_read) == test_content);
        }
    }

    // Cleanup
    std::filesystem::remove(temp_file);
}

#else

TEST_CASE("fd_source/fd_sink not available on non-Unix", "[io][fd_source][fd_sink]")
{
    // This test just ensures the test file compiles on non-Unix platforms
    REQUIRE(true);
}

#endif
