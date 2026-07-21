// Negative-compile check (see test/CMakeLists.txt): referencing a format
// argument that was not passed must be rejected at compile time by the typed
// mh::format API. (An unused EXTRA argument is legal in fmt/std::format, so a
// missing argument is the portable guaranteed-to-fail shape.)
#include <mh/text/format.hpp>

std::string format_bad()
{
	return mh::format("{} {}", 1);
}
