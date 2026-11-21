#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

int main(int argc, char* argv[])
{
    // No ambient dispatcher is registered here: the tests that need one create and register it themselves.
    int result = Catch::Session().run(argc, argv);

    return result;
}