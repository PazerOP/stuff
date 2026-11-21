#include "mh/utility.hpp"

#include <catch2/catch_all.hpp>

#include <string>
#include <vector>

TEST_CASE("mh::copy", "[utility]")
{
	// returns an independent copy: mutating it leaves the original alone
	const std::vector<int> original{ 1, 2, 3 };
	auto copied = mh::copy(original);
	copied.push_back(4);
	CHECK(original.size() == 3);
	CHECK(copied.size() == 4);

	// the typical use case: hand a copy to something that consumes rvalues,
	// without a verbose explicit temporary
	std::string source = "keep me";
	std::vector<std::string> sink;
	sink.push_back(mh::copy(source)); // picks push_back(string&&)
	CHECK(source == "keep me");
	CHECK(sink.back() == "keep me");
}
