#include <catch2/catch_test_macros.hpp>
#include <mh/io/pipe.hpp>
#include <mh/io/fd_source.hpp>
#include <mh/io/fd_sink.hpp>
#include <mh/process/process.hpp>
#include <mh/concurrency/dispatcher.hpp>

#ifdef __unix__
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>
#include <filesystem>

// Helper function to run async tests
template<typename Func>
void run_async_test(Func&& func)
{
    // Get the current thread's registered dispatcher
    auto& disp = mh::dispatcher::get();
    auto task = func();
    disp.run_while([&]() { return !task.is_ready(); });
}

#include "last_include.hpp"

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

TEST_CASE("pipe with child process communication", "[io][pipe]")
{
    SECTION("pipe to child process stdout")
    {
        run_async_test([]() -> mh::task<void> {
            auto pipe = mh::io::pipe::create();
            
            // Create process that outputs to stdout
            mh::process proc("echo", {"echo", "Hello from child process!"}, 
                            nullptr, pipe->in, nullptr);
            
            REQUIRE(proc.start());
            
            // Wait for process to complete first
            auto exit_code = co_await proc.wait_async();
            REQUIRE(exit_code == 0);
            
            // Now read from the pipe (data should be buffered)
            char buffer[1024] = {0};
            auto bytes_read = co_await pipe->out->read_async(buffer, sizeof(buffer) - 1);
            
            REQUIRE(bytes_read > 0);
            REQUIRE(std::string(buffer, bytes_read) == "Hello from child process!\n");
        });
    }
    
    SECTION("pipe to child process stdin")
    {
        run_async_test([]() -> mh::task<void> {
            auto pipe = mh::io::pipe::create();
            
            // Create process that reads from stdin and prints to stdout
            // Using echo since cat may buffer input
            mh::process proc("/bin/bash", {"/bin/bash", "-c", "read line && echo \"Received: $line\""}, 
                            pipe->out, nullptr, nullptr);
            
            REQUIRE(proc.start());
            
            // Write to child's stdin via pipe
            const std::string test_message = "Input for child process!\n";
            auto bytes_written = co_await pipe->in->write_async(test_message.data(), test_message.size());
            REQUIRE(bytes_written == test_message.size());
            
            // Close write end to signal EOF
            pipe->in->close();
            
            // Wait for process to complete
            auto exit_code = co_await proc.wait_async();
            REQUIRE(exit_code == 0);
        });
    }
    
    SECTION("bidirectional pipe communication with child")
    {
        run_async_test([]() -> mh::task<void> {
            auto stdin_pipe = mh::io::pipe::create();
            auto stdout_pipe = mh::io::pipe::create();
            
            // Use a simple echo command that reads one line and echoes it back
            mh::process proc("/bin/bash", {"/bin/bash", "-c", "read line && echo \"$line\""}, 
                            stdin_pipe->out, stdout_pipe->in, nullptr);
            
            REQUIRE(proc.start());
            
            // Close unused ends immediately after starting the process
            stdin_pipe->out->close();
            stdout_pipe->in->close();
            
            // Send data to child with newline for proper line reading
            const std::string test_message = "Echo this message!\n";
            auto bytes_written = co_await stdin_pipe->in->write_async(test_message.data(), test_message.size());
            REQUIRE(bytes_written == test_message.size());
            stdin_pipe->in->close(); // Signal EOF
            
            // Read response from child
            char buffer[1024] = {0};
            auto bytes_read = co_await stdout_pipe->out->read_async(buffer, sizeof(buffer) - 1);
            
            REQUIRE(bytes_read > 0);
            std::string received(buffer, bytes_read);
            // The bash script should echo back the line (without the input newline but with its own)
            REQUIRE(received == "Echo this message!\n");
            
            // Wait for process to complete
            auto exit_code = co_await proc.wait_async();
            REQUIRE(exit_code == 0);
        });
    }
}

TEST_CASE("pipe redirection scenarios", "[io][pipe]")
{
    SECTION("stderr redirection through pipe")
    {
        run_async_test([]() -> mh::task<void> {
            auto pipe = mh::io::pipe::create();
            
            // Use a command that writes to stderr - bash -c allows us to redirect
            mh::process proc("/bin/bash", {"/bin/bash", "-c", "echo 'Error message to stderr!' >&2"}, 
                            nullptr, nullptr, pipe->in);
            
            REQUIRE(proc.start());
            
            // Close write end in parent
            pipe->in->close();
            
            // Read from child's stderr
            char buffer[1024] = {0};
            auto bytes_read = co_await pipe->out->read_async(buffer, sizeof(buffer) - 1);
            
            REQUIRE(bytes_read > 0);
            REQUIRE(std::string(buffer, bytes_read) == "Error message to stderr!\n");
            
            auto exit_code = co_await proc.wait_async();
            REQUIRE(exit_code == 0);
        });
    }
    
    SECTION("multiple pipe chain simulation")
    {
        run_async_test([]() -> mh::task<void> {
            // Simulate: echo "hello" | wc -c
            auto pipe1 = mh::io::pipe::create(); // echo -> wc
            
            // First process: echo "hello"
            mh::process echo_proc("echo", {"echo", "hello"}, 
                                 nullptr, pipe1->in, nullptr);
            
            REQUIRE(echo_proc.start());
            pipe1->in->close(); // Close write end in parent
            
            // Second process: wc -c (count characters)
            auto pipe2 = mh::io::pipe::create();
            mh::process wc_proc("wc", {"wc", "-c"}, 
                               pipe1->out, pipe2->in, nullptr);
            
            REQUIRE(wc_proc.start());
            pipe1->out->close(); // Close read end after connecting to wc
            pipe2->in->close();  // Close write end in parent
            
            // Read result from wc
            char buffer[1024] = {0};
            auto bytes_read = co_await pipe2->out->read_async(buffer, sizeof(buffer) - 1);
            
            REQUIRE(bytes_read > 0);
            // wc -c outputs "6\n" for "hello\n" (6 characters including newline)
            std::string result(buffer, bytes_read);
            REQUIRE(result.find("6") != std::string::npos);
            
            // Wait for both processes
            auto echo_exit = co_await echo_proc.wait_async();
            auto wc_exit = co_await wc_proc.wait_async();
            REQUIRE(echo_exit == 0);
            REQUIRE(wc_exit == 0);
        });
    }
}

TEST_CASE("concurrent pipe operations", "[io][pipe]")
{
    SECTION("multiple threads writing to same pipe")
    {
        auto pipe = mh::io::pipe::create();
        const int num_threads = 4;
        const int messages_per_thread = 10;
        
        std::vector<std::thread> writers;
        std::atomic<int> completed_writes{0};
        
        // Start writer threads
        for (int t = 0; t < num_threads; ++t) {
            writers.emplace_back([&pipe, &completed_writes, t]() {
                for (int i = 0; i < messages_per_thread; ++i) {
                    std::string message = "Thread" + std::to_string(t) + "Msg" + std::to_string(i) + "\n";
                    auto bytes_written = pipe->in->write_async(message.data(), message.size()).get();
                    REQUIRE(bytes_written == message.size());
                    completed_writes++;
                }
            });
        }
        
        // Reader thread
        std::vector<std::string> received_messages;
        std::thread reader([&pipe, &received_messages]() {
            std::string accumulated_data;
            char buffer[1024];
            
            // Read until we get EOF
            while (true) {
                auto bytes_read = pipe->out->read_async(buffer, sizeof(buffer)).get();
                if (bytes_read == 0) break; // EOF
                
                accumulated_data.append(buffer, bytes_read);
            }
            
            // Split by newlines to get individual messages
            size_t pos = 0;
            while ((pos = accumulated_data.find('\n')) != std::string::npos) {
                received_messages.push_back(accumulated_data.substr(0, pos + 1));
                accumulated_data.erase(0, pos + 1);
            }
            // Add any remaining data if it doesn't end with newline
            if (!accumulated_data.empty()) {
                received_messages.push_back(accumulated_data);
            }
        });
        
        // Wait for all writers to complete
        for (auto& writer : writers) {
            writer.join();
        }
        
        pipe->in->close(); // Signal EOF to reader
        reader.join();
        
        // Verify we received the expected number of messages
        REQUIRE(received_messages.size() == num_threads * messages_per_thread);
        REQUIRE(completed_writes == num_threads * messages_per_thread);
    }
}

TEST_CASE("pipe connection utilities", "[io][pipe]")
{
    SECTION("connect_io with valid source and sink")
    {
        auto temp_file = std::filesystem::temp_directory_path() / "mh_test_connect_io.txt";
        
        // Create test file
        {
            std::ofstream file(temp_file);
            file << "test content";
        }
        
        auto source = mh::io::source::create_file(temp_file);
        auto pipe_obj = mh::io::pipe::create();
        
        auto result = mh::io::connect_io(source, pipe_obj->in);
        REQUIRE(result != nullptr);
        
        // Cleanup
        std::filesystem::remove(temp_file);
    }
    
    SECTION("connect_io with null pointers")
    {
        auto pipe_obj = mh::io::pipe::create();
        
        REQUIRE(mh::io::connect_io(nullptr, pipe_obj->in) == nullptr);
        REQUIRE(mh::io::connect_io(pipe_obj->out, nullptr) == nullptr);
        REQUIRE(mh::io::connect_io(nullptr, nullptr) == nullptr);
    }
    
    SECTION("connect_io with closed streams")
    {
        auto pipe1 = mh::io::pipe::create();
        auto pipe2 = mh::io::pipe::create();
        
        // Close one stream
        pipe1->out->close();
        
        REQUIRE(mh::io::connect_io(pipe1->out, pipe2->in) == nullptr);
    }
    
    SECTION("connect_io with same file descriptor")
    {
        auto pipe_obj = mh::io::pipe::create();
        
        // Connecting same source/sink should work
        auto result = mh::io::connect_io(pipe_obj->out, pipe_obj->in);
        REQUIRE(result != nullptr);
    }
}

#else

TEST_CASE("pipe not available on non-Unix", "[io][pipe]")
{
    // This test just ensures the test file compiles on non-Unix platforms
    REQUIRE(true);
}

#endif