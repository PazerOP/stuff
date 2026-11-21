// Positive control for the negative-compile check in test/CMakeLists.txt: a
// valid literal format string MUST compile, proving the harness can tell a
// working build from a broken one.
#include <mh/text/format.hpp>

std::string format_good()
{
	return mh::format("{} {}", 1, "x");
}
