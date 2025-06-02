#include <catch2/catch_test_macros.hpp>
#include <mh/io/source.hpp>
#include <mh/io/sink.hpp>

#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>

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


#else

TEST_CASE("source/sink not available on non-Unix", "[io][source][sink]")
{
    // This test just ensures the test file compiles on non-Unix platforms
    REQUIRE(true);
}

#endif