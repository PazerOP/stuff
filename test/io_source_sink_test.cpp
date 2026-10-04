#include <catch2/catch_test_macros.hpp>
#include <mh/io/source.hpp>
#include <mh/io/sink.hpp>
#include <mh/io/fd_source.hpp>
#include <mh/io/fd_sink.hpp>

#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include "last_include.hpp"

TEST_CASE("source static factory methods", "[io][source]")
{
    SECTION("stdout_source returns valid source")
    {
        auto src = mh::io::source::stdout_source();
        REQUIRE(src != nullptr);
        REQUIRE(src->get_native_handle() >= 0);
    }
    
    SECTION("stderr_source returns valid source") 
    {
        auto src = mh::io::source::stderr_source();
        REQUIRE(src != nullptr);
        REQUIRE(src->get_native_handle() >= 0);
    }
    
    SECTION("stdout_source returns singleton")
    {
        auto src1 = mh::io::source::stdout_source();
        auto src2 = mh::io::source::stdout_source();
        REQUIRE(src1.get() == src2.get()); // Same instance
    }
    
    SECTION("stderr_source returns singleton")
    {
        auto src1 = mh::io::source::stderr_source();
        auto src2 = mh::io::source::stderr_source();
        REQUIRE(src1.get() == src2.get()); // Same instance
    }
}

TEST_CASE("sink static factory methods", "[io][sink]")
{
    SECTION("stdin_sink returns valid sink")
    {
        auto snk = mh::io::sink::stdin_sink();
        REQUIRE(snk != nullptr);
        REQUIRE(snk->get_native_handle() >= 0);
    }
    
    SECTION("stdin_sink returns singleton")
    {
        auto snk1 = mh::io::sink::stdin_sink();
        auto snk2 = mh::io::sink::stdin_sink();
        REQUIRE(snk1.get() == snk2.get()); // Same instance
    }
}

TEST_CASE("source interface consistency", "[io][source]")
{
    SECTION("standard stream sources are open by default")
    {
        auto stdout_src = mh::io::source::stdout_source();
        auto stderr_src = mh::io::source::stderr_source();
        
        REQUIRE(stdout_src->is_open());
        REQUIRE(stderr_src->is_open());
    }
    
    SECTION("standard stream sources have valid handles")
    {
        auto stdout_src = mh::io::source::stdout_source();
        auto stderr_src = mh::io::source::stderr_source();
        
        REQUIRE(stdout_src->get_native_handle() >= 0);
        REQUIRE(stderr_src->get_native_handle() >= 0);
        REQUIRE(stdout_src->get_native_handle() != stderr_src->get_native_handle());
    }
}

TEST_CASE("sink interface consistency", "[io][sink]")
{
    SECTION("standard stream sink is open by default")
    {
        auto stdin_snk = mh::io::sink::stdin_sink();
        REQUIRE(stdin_snk->is_open());
    }
    
    SECTION("standard stream sink has valid handle")
    {
        auto stdin_snk = mh::io::sink::stdin_sink();
        REQUIRE(stdin_snk->get_native_handle() >= 0);
    }
}

TEST_CASE("actual I/O operations with standard streams", "[io][source][sink]")
{
    SECTION("stdout source can read from stdout pipe")
    {
        int pipefd[2];
        REQUIRE(pipe(pipefd) == 0);
        
        const std::string test_data = "Hello from stdout test!\n";
        
        // Write test data to pipe write end
        REQUIRE(write(pipefd[1], test_data.c_str(), test_data.size()) == static_cast<ssize_t>(test_data.size()));
        close(pipefd[1]); // Close write end
        
        // Create source from pipe read end
        mh::io::fd_source pipe_source(pipefd[0], true);
        
        // Read from source
        char buffer[256] = {0};
        auto bytes_read = pipe_source.read_async(buffer, sizeof(buffer) - 1).get();
        
        REQUIRE(bytes_read == test_data.size());
        REQUIRE(std::string(buffer, bytes_read) == test_data);
    }
    
    SECTION("stderr source can read from stderr pipe")
    {
        int pipefd[2];
        REQUIRE(pipe(pipefd) == 0);
        
        const std::string test_data = "Error message from stderr!\n";
        
        // Write test data to pipe write end
        REQUIRE(write(pipefd[1], test_data.c_str(), test_data.size()) == static_cast<ssize_t>(test_data.size()));
        close(pipefd[1]); // Close write end
        
        // Create source from pipe read end
        mh::io::fd_source pipe_source(pipefd[0], true);
        
        // Read from source
        char buffer[256] = {0};
        auto bytes_read = pipe_source.read_async(buffer, sizeof(buffer) - 1).get();
        
        REQUIRE(bytes_read == test_data.size());
        REQUIRE(std::string(buffer, bytes_read) == test_data);
    }
    
    SECTION("stdin sink can write to stdin pipe")
    {
        int pipefd[2];
        REQUIRE(pipe(pipefd) == 0);
        
        const std::string test_data = "Input data for stdin!\n";
        
        // Create sink from pipe write end
        mh::io::fd_sink pipe_sink(pipefd[1], true);
        
        // Write to sink
        auto bytes_written = pipe_sink.write_async(test_data.c_str(), test_data.size()).get();
        REQUIRE(bytes_written == test_data.size());
        
        pipe_sink.close(); // Close write end to signal EOF
        
        // Read from pipe read end to verify
        char buffer[256] = {0};
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        
        REQUIRE(bytes_read == static_cast<ssize_t>(test_data.size()));
        REQUIRE(std::string(buffer, bytes_read) == test_data);
        
        close(pipefd[0]); // Clean up read end
    }
    
    SECTION("standard stream sources handle multiple reads")
    {
        int pipefd[2];
        REQUIRE(pipe(pipefd) == 0);
        
        const std::string first_chunk = "First chunk\n";
        const std::string second_chunk = "Second chunk\n";
        
        // Write data in chunks
        REQUIRE(write(pipefd[1], first_chunk.c_str(), first_chunk.size()) == static_cast<ssize_t>(first_chunk.size()));
        REQUIRE(write(pipefd[1], second_chunk.c_str(), second_chunk.size()) == static_cast<ssize_t>(second_chunk.size()));
        close(pipefd[1]); // Close write end
        
        // Create source from pipe read end
        mh::io::fd_source pipe_source(pipefd[0], true);
        
        // Read first chunk
        char buffer1[32] = {0};
        auto bytes_read1 = pipe_source.read_async(buffer1, first_chunk.size()).get();
        REQUIRE(bytes_read1 == first_chunk.size());
        REQUIRE(std::string(buffer1, bytes_read1) == first_chunk);
        
        // Read second chunk
        char buffer2[32] = {0};
        auto bytes_read2 = pipe_source.read_async(buffer2, second_chunk.size()).get();
        REQUIRE(bytes_read2 == second_chunk.size());
        REQUIRE(std::string(buffer2, bytes_read2) == second_chunk);
        
        // Reading beyond EOF should return 0
        char buffer3[32] = {0};
        auto bytes_read3 = pipe_source.read_async(buffer3, sizeof(buffer3)).get();
        REQUIRE(bytes_read3 == 0);
    }
    
    SECTION("sink handles large writes")
    {
        int pipefd[2];
        REQUIRE(pipe(pipefd) == 0);
        
        // Create large test data (8KB)
        std::string large_data;
        large_data.reserve(8192);
        for (int i = 0; i < 8192; ++i) {
            large_data += static_cast<char>('A' + (i % 26));
        }
        
        // Create sink from pipe write end
        mh::io::fd_sink pipe_sink(pipefd[1], true);
        
        // Write large data
        auto bytes_written = pipe_sink.write_async(large_data.c_str(), large_data.size()).get();
        REQUIRE(bytes_written == large_data.size());
        
        pipe_sink.close(); // Close write end
        
        // Read back and verify
        std::string read_data;
        read_data.resize(large_data.size());
        ssize_t total_read = 0;
        ssize_t bytes_read;
        
        while (total_read < static_cast<ssize_t>(large_data.size())) {
            bytes_read = read(pipefd[0], &read_data[total_read], large_data.size() - total_read);
            if (bytes_read <= 0) break;
            total_read += bytes_read;
        }
        
        REQUIRE(total_read == static_cast<ssize_t>(large_data.size()));
        REQUIRE(read_data == large_data);
        
        close(pipefd[0]); // Clean up read end
    }
    
    SECTION("source and sink error handling")
    {
        // Test reading from closed file descriptor
        int fd = open("/dev/null", O_RDONLY);
        REQUIRE(fd >= 0);
        close(fd); // Close immediately
        
        mh::io::fd_source closed_source(fd, false); // Don't take ownership of already closed fd
        
        char buffer[10];
        REQUIRE_THROWS(closed_source.read_async(buffer, sizeof(buffer)).get());
    }
}


#else

TEST_CASE("source/sink not available on non-Unix", "[io][source][sink]")
{
    // This test just ensures the test file compiles on non-Unix platforms
    REQUIRE(true);
}

#endif