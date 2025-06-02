#include <catch2/catch_test_macros.hpp>
#include <mh/io/pipe.hpp>
#include <mh/io/fd_source.hpp>
#include <mh/io/fd_sink.hpp>

#ifdef __unix__
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

TEST_CASE("pipe creation", "[io][pipe]")
{
    SECTION("create returns valid pipe")
    {
        auto pipe = mh::io::pipe::create();
        
        REQUIRE(pipe != nullptr);
        REQUIRE(pipe->in != nullptr);
        REQUIRE(pipe->out != nullptr);
    }
    
    SECTION("pipe ends are open after creation")
    {
        auto pipe = mh::io::pipe::create();
        
        REQUIRE(pipe->in->is_open());
        REQUIRE(pipe->out->is_open());
    }
    
    SECTION("pipe ends have different file descriptors")
    {
        auto pipe = mh::io::pipe::create();
        
        auto read_fd = pipe->out->get_native_handle();
        auto write_fd = pipe->in->get_native_handle();
        
        REQUIRE(read_fd >= 0);
        REQUIRE(write_fd >= 0);
        REQUIRE(read_fd != write_fd);
    }
}

TEST_CASE("pipe data transfer", "[io][pipe]")
{
    SECTION("simple write and read")
    {
        auto pipe = mh::io::pipe::create();
        const std::string test_data = "Hello, pipe!";
        
        // Write data to pipe
        auto write_task = pipe->in->write_async(test_data.data(), test_data.size());
        auto bytes_written = write_task.get();
        REQUIRE(bytes_written == test_data.size());
        
        // Read data from pipe
        char buffer[1024] = {0};
        auto read_task = pipe->out->read_async(buffer, sizeof(buffer) - 1);
        auto bytes_read = read_task.get();
        
        REQUIRE(bytes_read == test_data.size());
        REQUIRE(std::string(buffer, bytes_read) == test_data);
    }
    
    SECTION("multiple writes and reads")
    {
        auto pipe = mh::io::pipe::create();
        const std::string test_data1 = "First message\n";
        const std::string test_data2 = "Second message\n";
        
        // Write first message
        auto bytes_written1 = pipe->in->write_async(test_data1.data(), test_data1.size()).get();
        REQUIRE(bytes_written1 == test_data1.size());
        
        // Write second message
        auto bytes_written2 = pipe->in->write_async(test_data2.data(), test_data2.size()).get();
        REQUIRE(bytes_written2 == test_data2.size());
        
        // Read both messages
        char buffer[1024] = {0};
        auto bytes_read = pipe->out->read_async(buffer, sizeof(buffer) - 1).get();
        
        std::string received(buffer, bytes_read);
        REQUIRE(received == test_data1 + test_data2);
    }
    
    SECTION("large data transfer")
    {
        auto pipe = mh::io::pipe::create();
        
        // Create a larger test message (1KB)
        std::string large_data;
        large_data.reserve(1024);
        for (int i = 0; i < 128; ++i) {
            large_data += "ABCDEFGH";
        }
        
        // Write large data
        auto bytes_written = pipe->in->write_async(large_data.data(), large_data.size()).get();
        REQUIRE(bytes_written == large_data.size());
        
        // Read large data
        char buffer[2048] = {0};
        auto bytes_read = pipe->out->read_async(buffer, sizeof(buffer) - 1).get();
        
        REQUIRE(bytes_read == large_data.size());
        REQUIRE(std::string(buffer, bytes_read) == large_data);
    }
}

TEST_CASE("pipe EOF behavior", "[io][pipe]")
{
    SECTION("closing write end signals EOF to read end")
    {
        auto pipe = mh::io::pipe::create();
        const std::string test_data = "Before EOF";
        
        // Write some data
        auto bytes_written = pipe->in->write_async(test_data.data(), test_data.size()).get();
        REQUIRE(bytes_written == test_data.size());
        
        // Close write end
        pipe->in->close();
        REQUIRE(!pipe->in->is_open());
        
        // Read should get the data
        char buffer[1024] = {0};
        auto bytes_read = pipe->out->read_async(buffer, sizeof(buffer) - 1).get();
        REQUIRE(bytes_read == test_data.size());
        REQUIRE(std::string(buffer, bytes_read) == test_data);
        
        // Subsequent read should return 0 (EOF)
        auto eof_read = pipe->out->read_async(buffer, sizeof(buffer) - 1).get();
        REQUIRE(eof_read == 0);
    }
}

TEST_CASE("pipe error conditions", "[io][pipe]")
{
    SECTION("writing to closed pipe throws")
    {
        auto pipe = mh::io::pipe::create();
        
        pipe->in->close();
        REQUIRE(!pipe->in->is_open());
        
        const std::string test_data = "This should fail";
        REQUIRE_THROWS_AS(
            pipe->in->write_async(test_data.data(), test_data.size()).get(),
            std::runtime_error
        );
    }
    
    SECTION("reading from closed pipe throws")
    {
        auto pipe = mh::io::pipe::create();
        
        pipe->out->close();
        REQUIRE(!pipe->out->is_open());
        
        char buffer[1024];
        REQUIRE_THROWS_AS(
            pipe->out->read_async(buffer, sizeof(buffer)).get(),
            std::runtime_error
        );
    }
}

TEST_CASE("pipe constructor", "[io][pipe]")
{
    SECTION("constructor with custom source and sink")
    {
        auto system_pipe = mh::io::pipe::create();
        auto read_end = system_pipe->out;
        auto write_end = system_pipe->in;
        
        // Create pipe object from existing source/sink
        auto custom_pipe = std::make_shared<mh::io::pipe>(read_end, write_end);
        
        REQUIRE(custom_pipe->out.get() == read_end.get());
        REQUIRE(custom_pipe->in.get() == write_end.get());
    }
}

TEST_CASE("pipe naming convention", "[io][pipe]")
{
    SECTION("pipe naming follows expected convention")
    {
        auto pipe = mh::io::pipe::create();
        
        // 'in' should be the sink (for writing into the pipe)
        // 'out' should be the source (for reading out of the pipe)
        REQUIRE(pipe->in != nullptr);  // sink for writing
        REQUIRE(pipe->out != nullptr); // source for reading
        
        // Test that the naming is consistent with usage
        const std::string test_data = "Naming test";
        
        // Write to 'in' (sink)
        auto bytes_written = pipe->in->write_async(test_data.data(), test_data.size()).get();
        REQUIRE(bytes_written == test_data.size());
        
        // Read from 'out' (source)
        char buffer[1024] = {0};
        auto bytes_read = pipe->out->read_async(buffer, sizeof(buffer) - 1).get();
        REQUIRE(bytes_read == test_data.size());
        REQUIRE(std::string(buffer, bytes_read) == test_data);
    }
}

TEST_CASE("pipe resource management", "[io][pipe]")
{
    SECTION("pipe destruction cleans up file descriptors")
    {
        mh::io::native_handle read_fd, write_fd;
        
        {
            auto pipe = mh::io::pipe::create();
            read_fd = pipe->out->get_native_handle();
            write_fd = pipe->in->get_native_handle();
            
            REQUIRE(read_fd >= 0);
            REQUIRE(write_fd >= 0);
        }
        // Pipe should be destroyed here, cleaning up file descriptors
        
        // Try to use the file descriptors directly - should fail
        char buffer[1];
        ssize_t read_result = read(read_fd, buffer, 1);
        REQUIRE(read_result == -1); // Should fail because fd is closed
        
        const char test_char = 'x';
        ssize_t write_result = write(write_fd, &test_char, 1);
        REQUIRE(write_result == -1); // Should fail because fd is closed
    }
}

#else

TEST_CASE("pipe not available on non-Unix", "[io][pipe]")
{
    // This test just ensures the test file compiles on non-Unix platforms
    REQUIRE(true);
}

#endif