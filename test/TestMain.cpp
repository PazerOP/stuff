#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <mh/concurrency/dispatcher.hpp>
#include "last_include.hpp"

int main(int argc, char* argv[])
{
    // Set up the dispatcher for the test thread
    mh::dispatcher test_dispatcher;
    test_dispatcher.register_for_current_thread();
    
    // Run the Catch2 tests
    int result = Catch::Session().run(argc, argv);
    
    return result;
}