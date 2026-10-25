// Second translation unit for text_format_test. Formatting a source_location
// from more than one TU must compile AND link in both header-only and library
// builds. This file intentionally contains no test cases of its own.
#include "mh/text/format.hpp"
#include "mh/source_location.hpp"

#include <string>
#include "last_include.hpp"

#if MH_FORMATTER != MH_FORMATTER_NONE

std::string format_source_location_from_second_tu()
{
	return mh::format("{}", MH_SOURCE_LOCATION_CURRENT());
}

#endif // MH_FORMATTER != MH_FORMATTER_NONE
