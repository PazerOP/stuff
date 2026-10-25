#include <catch2/catch_test_macros.hpp>
#include <mh/io/native_handle.hpp>

#ifdef __unix__
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include "last_include.hpp"

static size_t write_safe(int fd, const void* buf, size_t count)
{
    ssize_t result = ::write(fd, buf, count);
    if (result < 0)
        throw std::runtime_error("write failed");
    return static_cast<size_t>(result);
}

static size_t read_safe(int fd, void* buf, size_t count)
{
    ssize_t result = ::read(fd, buf, count);
    if (result < 0)
        throw std::runtime_error("read failed");
    return static_cast<size_t>(result);
}

TEST_CASE("fd_traits basic functionality", "[io][native_handle]")
{
    using traits = mh::io::detail::native_handle_hpp::fd_traits;
    
    SECTION("invalid value")
    {
        REQUIRE(traits::invalid() == -1);
    }
    
    SECTION("is_obj_valid")
    {
        traits t;
        REQUIRE(t.is_obj_valid(0) == true);   // stdin
        REQUIRE(t.is_obj_valid(1) == true);   // stdout  
        REQUIRE(t.is_obj_valid(2) == true);   // stderr
        REQUIRE(t.is_obj_valid(10) == true);  // any positive fd
        REQUIRE(t.is_obj_valid(-1) == false); // invalid fd
        REQUIRE(t.is_obj_valid(-5) == false); // negative fd
    }
}

TEST_CASE("fd_traits file operations", "[io][native_handle]")
{
    using traits = mh::io::detail::native_handle_hpp::fd_traits;
    traits t;
    
    SECTION("delete_obj with valid fd")
    {
        // Create a temporary file to get a valid fd
        auto temp_file = std::filesystem::temp_directory_path() / "mh_test_fd.txt";
        int fd = open(temp_file.c_str(), O_CREAT | O_WRONLY, 0644);
        REQUIRE(fd >= 0);
        
        // delete_obj should close the fd
        t.delete_obj(fd);
        
        // Try to write to closed fd - should fail
        char buffer[] = "test";
        ssize_t result = write(fd, buffer, sizeof(buffer));
        REQUIRE(result == -1);
        
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("delete_obj with invalid fd")
    {
        // Should not crash when called with invalid fd
        int invalid1 = -1;
        int invalid2 = -100;
        REQUIRE_NOTHROW(t.delete_obj(invalid1));
        REQUIRE_NOTHROW(t.delete_obj(invalid2));
    }
    
    SECTION("release_obj")
    {
        int fd = 42;
        int released = t.release_obj(fd);
        
        REQUIRE(released == 42);
        REQUIRE(fd == traits::invalid());
    }
}

TEST_CASE("unique_native_handle construction", "[io][native_handle]")
{
    SECTION("default construction")
    {
        mh::io::unique_native_handle handle;
        REQUIRE(!handle);
        REQUIRE(handle.value() == -1);
    }
    
    SECTION("construction with valid fd")
    {
        auto temp_file = std::filesystem::temp_directory_path() / "mh_test_unique_fd.txt";
        int fd = open(temp_file.c_str(), O_CREAT | O_WRONLY, 0644);
        REQUIRE(fd >= 0);
        
        mh::io::unique_native_handle handle(fd);
        REQUIRE(handle);
        REQUIRE(handle.value() == fd);
        
        // Handle will automatically close fd when destroyed
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("construction with invalid fd")
    {
        mh::io::unique_native_handle handle(-1);
        REQUIRE(!handle);
        REQUIRE(handle.value() == -1);
    }
}

TEST_CASE("unique_native_handle move semantics", "[io][native_handle]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_move_fd.txt";
    int fd = open(temp_file.c_str(), O_CREAT | O_WRONLY, 0644);
    REQUIRE(fd >= 0);
    
    SECTION("move construction")
    {
        mh::io::unique_native_handle handle1(fd);
        REQUIRE(handle1);
        
        mh::io::unique_native_handle handle2(std::move(handle1));
        REQUIRE(handle2);
        REQUIRE(handle2.value() == fd);
        REQUIRE(!handle1);
    }
    
    SECTION("move assignment")
    {
        mh::io::unique_native_handle handle1(fd);
        mh::io::unique_native_handle handle2;
        
        REQUIRE(handle1);
        REQUIRE(!handle2);
        
        handle2 = std::move(handle1);
        REQUIRE(handle2);
        REQUIRE(handle2.value() == fd);
        REQUIRE(!handle1);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("unique_native_handle release and reset", "[io][native_handle]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_release_fd.txt";
    int fd = open(temp_file.c_str(), O_CREAT | O_WRONLY, 0644);
    REQUIRE(fd >= 0);
    
    SECTION("release")
    {
        mh::io::unique_native_handle handle(fd);
        REQUIRE(handle);
        
        int released_fd = handle.release();
        REQUIRE(released_fd == fd);
        REQUIRE(!handle);
        
        // We need to manually close the released fd
        close(released_fd);
    }
    
    SECTION("reset with new fd")
    {
        mh::io::unique_native_handle handle(fd);
        
        // Create another fd
        auto temp_file2 = std::filesystem::temp_directory_path() / "mh_test_reset_fd2.txt";
        int fd2 = open(temp_file2.c_str(), O_CREAT | O_WRONLY, 0644);
        REQUIRE(fd2 >= 0);
        
        handle.reset(fd2);
        REQUIRE(handle);
        REQUIRE(handle.value() == fd2);
        
        // Original fd should be closed, fd2 will be closed by handle destructor
        std::filesystem::remove(temp_file2);
    }
    
    SECTION("reset to invalid")
    {
        mh::io::unique_native_handle handle(fd);
        REQUIRE(handle);
        
        handle.reset();
        REQUIRE(!handle);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("unique_native_handle RAII behavior", "[io][native_handle]")
{
    auto temp_file = std::filesystem::temp_directory_path() / "mh_test_raii_fd.txt";
    int fd;
    
    SECTION("automatic cleanup on scope exit")
    {
        fd = open(temp_file.c_str(), O_CREAT | O_WRONLY, 0644);
        REQUIRE(fd >= 0);
        
        {
            mh::io::unique_native_handle handle(fd);
            REQUIRE(handle);
            // Handle will automatically close fd when going out of scope
        }
        
        // Try to write to the fd - should fail because it's closed
        char buffer[] = "test";
        ssize_t result = write(fd, buffer, sizeof(buffer));
        REQUIRE(result == -1);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("unique_native_handle with pipe", "[io][native_handle]")
{
    SECTION("pipe file descriptors")
    {
        int pipefd[2];
        REQUIRE(pipe(pipefd) == 0);
        
        mh::io::unique_native_handle read_end(pipefd[0]);
        mh::io::unique_native_handle write_end(pipefd[1]);
        
        REQUIRE(read_end);
        REQUIRE(write_end);
        
        // Test writing and reading
        const char* message = "test message";
        size_t written = write_safe(write_end.value(), message, strlen(message));
        REQUIRE(written == strlen(message));

        write_end.reset(); // Close write end to signal EOF

        char buffer[100] = {0};
        size_t read_bytes = read_safe(read_end.value(), buffer, sizeof(buffer) - 1);
        REQUIRE(read_bytes == strlen(message));
        REQUIRE(strcmp(buffer, message) == 0);
        
        // Handles will automatically close the pipe fds
    }
}

#else

TEST_CASE("native_handle not available on non-Unix", "[io][native_handle]")
{
    // This test just ensures the test file compiles on non-Unix platforms
    REQUIRE(true);
}

#endif