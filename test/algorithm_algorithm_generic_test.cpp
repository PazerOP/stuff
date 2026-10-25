#include "mh/algorithm/algorithm_generic.hpp"

#include <catch2/catch_all.hpp>

#include <map>
#include <string>
#include <vector>

TEST_CASE("for_each_multimap_group", "[algorithm]")
{
	std::multimap<int, std::string> map;
	map.insert({ 1, "a" });
	map.insert({ 1, "b" });
	map.insert({ 3, "c" });
	map.insert({ 7, "d" });
	map.insert({ 7, "e" });
	map.insert({ 7, "f" });

	// each key visited exactly once, with its full equal_range
	std::vector<std::pair<int, std::string>> groups;
	mh::for_each_multimap_group(map, [&](int key, auto begin, auto end)
		{
			std::string values;
			for (auto it = begin; it != end; ++it)
				values += it->second;

			groups.emplace_back(key, std::move(values));
		});

	REQUIRE(groups.size() == 3);
	CHECK(groups[0] == std::pair{ 1, std::string("ab") });
	CHECK(groups[1] == std::pair{ 3, std::string("c") });
	CHECK(groups[2] == std::pair{ 7, std::string("def") });
}

TEST_CASE("for_each_multimap_group - empty and single-key containers", "[algorithm]")
{
	std::multimap<int, int> map;

	int calls = 0;
	mh::for_each_multimap_group(map, [&](int, auto, auto) { calls++; });
	CHECK(calls == 0);

	map.insert({ 5, 1 });
	map.insert({ 5, 2 });
	mh::for_each_multimap_group(map, [&](int key, auto begin, auto end)
		{
			calls++;
			CHECK(key == 5);
			CHECK(std::distance(begin, end) == 2);
		});
	CHECK(calls == 1);
}
