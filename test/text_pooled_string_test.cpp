#include "mh/text/pooled_string.hpp"
#include <catch2/catch_all.hpp>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "last_include.hpp"

using mh::pooled_string;
using namespace std::string_literals;
using namespace std::string_view_literals;

static_assert(sizeof(pooled_string) == sizeof(const char*));
static_assert(std::is_trivially_copyable_v<pooled_string>);
static_assert(std::is_trivially_destructible_v<pooled_string>);
static_assert(!std::is_convertible_v<std::string_view, pooled_string>, "a construction reads the pool, so it must be explicit");
static_assert(pooled_string().empty());
static_assert(pooled_string() == pooled_string());

TEST_CASE("pooled_string - empty", "[text][pooled_string]")
{
	const pooled_string def;
	const pooled_string empty("");

	CHECK(def.empty());
	CHECK(def.size() == 0);
	CHECK(def.c_str() == ""s);
	CHECK(def.view() == ""sv);
	CHECK(def.begin() == def.end());

	CHECK(empty.empty());
	CHECK(def == empty);
	CHECK(def.data() == empty.data());
	CHECK(def.hash() == empty.hash());
}

TEST_CASE("pooled_string - equal strings share one pointer", "[text][pooled_string]")
{
	// Separate buffers, so that equal pointers can only come from the pool.
	std::string buf1 = "hello pooled world";
	std::string buf2 = "hello pooled world";
	REQUIRE(buf1.data() != buf2.data());

	const pooled_string a(buf1);
	const pooled_string b(buf2);
	const pooled_string c("hello pooled worle");

	CHECK(a == b);
	CHECK(a.data() == b.data());
	CHECK(a.hash() == b.hash());
	CHECK(std::hash<pooled_string>()(a) == std::hash<pooled_string>()(b));

	CHECK(a != c);
	CHECK(a.data() != c.data());

	// The pool owns a copy.
	CHECK(a.data() != buf1.data());
	buf1[0] = 'J';
	CHECK(a.view() == "hello pooled world");
}

TEST_CASE("pooled_string - contents", "[text][pooled_string]")
{
	const pooled_string s("abc");
	CHECK(!s.empty());
	CHECK(s.size() == 3);
	CHECK(s.c_str()[3] == '\0');
	CHECK(std::string(s.begin(), s.end()) == "abc");
	CHECK(std::string_view(s) == "abc");

	SECTION("a prefix is a different string")
	{
		CHECK(pooled_string("ab") != s);
		CHECK(pooled_string("abcd") != s);
		CHECK(pooled_string("abcd"sv.substr(0, 3)) == s);
	}

	SECTION("utf-8 bytes survive")
	{
		const std::string_view utf8 = "h\xC3\xA9llo \xE2\x82\xAC \xF0\x9F\x98\x80";
		const pooled_string p(utf8);
		CHECK(p.size() == utf8.size());
		CHECK(p.view() == utf8);
		CHECK(p == pooled_string(std::string(utf8)));
	}

	SECTION("embedded null")
	{
		const pooled_string p("a\0b"sv);
		CHECK(p.size() == 3);
		CHECK(p.view() == "a\0b"sv);
		CHECK(p != pooled_string("a"));
		CHECK(p != pooled_string("a\0c"sv));
	}

	SECTION("long string")
	{
		// Longer than one pool block.
		std::string big(200'000, 'x');
		big[100'000] = 'y';
		const pooled_string p(big);
		CHECK(p.size() == big.size());
		CHECK(p.view() == big);
		CHECK(p.c_str()[big.size()] == '\0');
		CHECK(p.data() == pooled_string(big).data());

		big[100'000] = 'z';
		CHECK(p != pooled_string(big));
	}
}

TEST_CASE("pooled_string - comparison", "[text][pooled_string]")
{
	const pooled_string apple("apple"), banana("banana"), empty;

	CHECK(apple == "apple");
	CHECK("apple" == apple);
	CHECK(apple == "apple"s);
	CHECK(apple == "apple"sv);
	CHECK(apple != "apple pie");
	CHECK(empty == "");

	CHECK(apple < banana);
	CHECK(banana > apple);
	CHECK(apple <= apple);
	CHECK(empty < apple);
	CHECK((apple <=> pooled_string("apple")) == std::strong_ordering::equal);
	CHECK(pooled_string("app") < apple);

	// Bytes above 0x7F order as unsigned char.
	CHECK(pooled_string("z") < pooled_string("\xC3\xA9"));

	std::vector<pooled_string> v{ pooled_string("pear"), banana, empty, apple };
	std::sort(v.begin(), v.end());
	CHECK(v == std::vector<pooled_string>{ empty, apple, banana, pooled_string("pear") });
}

TEST_CASE("pooled_string - containers and streams", "[text][pooled_string]")
{
	std::unordered_map<pooled_string, int> counts;
	for (const char* word : { "the", "quick", "the", "lazy", "the", "quick", "" })
		counts[pooled_string(word)]++;

	CHECK(counts.size() == 4);
	CHECK(counts.at(pooled_string("the")) == 3);
	CHECK(counts.at(pooled_string("quick")) == 2);
	CHECK(counts.at(pooled_string("lazy")) == 1);
	CHECK(counts.at(pooled_string()) == 1);

	std::map<pooled_string, int> ordered(counts.begin(), counts.end());
	CHECK(ordered.begin()->first.empty());
	CHECK(std::next(ordered.begin())->first == "lazy");

	std::ostringstream ss;
	ss << pooled_string("left") << '|' << std::setw(6) << pooled_string("pad") << '|' << pooled_string();
	CHECK(ss.str() == "left|   pad|");
}

TEST_CASE("pooled_string - many strings", "[text][pooled_string]")
{
	// Enough strings to grow the table and to fill many pool blocks.
	constexpr int COUNT = 100'000;
	const auto make = [](int i) { return "many/" + std::to_string(i) + std::string(static_cast<std::size_t>(i % 40), '#'); };

	const pooled_string early(make(7));

	std::vector<pooled_string> first;
	first.reserve(COUNT);
	for (int i = 0; i < COUNT; i++)
		first.emplace_back(make(i));

	CHECK(first[7].data() == early.data());

	std::unordered_set<const char*> pointers;
	std::unordered_set<std::size_t> hashes;
	for (pooled_string s : first)
	{
		pointers.insert(s.data());
		hashes.insert(s.hash());
	}
	CHECK(pointers.size() == COUNT);
	CHECK(hashes.size() == COUNT);

	bool allSame = true;
	for (int i = 0; i < COUNT; i++)
	{
		const std::string expected = make(i);
		allSame &= first[static_cast<std::size_t>(i)].view() == expected;
		allSame &= first[static_cast<std::size_t>(i)].data() == pooled_string(expected).data();
	}
	CHECK(allSame);
}

TEST_CASE("pooled_string - threads", "[text][pooled_string]")
{
	// Every thread constructs the same new strings at the same time. This test adds more strings
	// than all the other tests together, so the table must grow while the threads read it.
	constexpr int THREADS = 8;
	constexpr int COUNT = 200'000;
	const auto make = [](int i) { return "threads/" + std::to_string(i); };

	std::vector<std::vector<pooled_string>> results(THREADS);
	{
		std::vector<std::thread> threads;
		for (auto& result : results)
		{
			threads.emplace_back([&result, &make]
				{
					result.reserve(COUNT);
					for (int i = 0; i < COUNT; i++)
						result.emplace_back(make(i));
				});
		}

		for (auto& thread : threads)
			thread.join();
	}

	bool contentsOk = true;
	for (int i = 0; i < COUNT; i++)
		contentsOk &= results[0][static_cast<std::size_t>(i)].view() == make(i);
	CHECK(contentsOk);

	for (int t = 1; t < THREADS; t++)
		CHECK(results[static_cast<std::size_t>(t)] == results[0]);
}
