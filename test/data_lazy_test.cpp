#include "mh/data/lazy.hpp"

#include <catch2/catch_all.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

TEST_CASE("lazy - type aliases", "[data][lazy]")
{
	using int_func = int(*)();
	using lz = mh::lazy<int_func>;
	STATIC_CHECK(std::is_same_v<lz::func_type, int_func>);
	STATIC_CHECK(std::is_same_v<lz::value_type, int>);
}

TEST_CASE("lazy - empty lazy throws on access", "[data][lazy]")
{
	const mh::lazy<int(*)()> empty;
	REQUIRE_THROWS_MATCHES(empty.get(), std::logic_error, Catch::Matchers::Message("Empty mh::lazy"));
	REQUIRE_THROWS_AS(empty(), std::logic_error);
}

TEST_CASE("lazy - invokes on first access only", "[data][lazy]")
{
	int calls = 0;
	const mh::lazy lz([&calls]
		{
			++calls;
			return std::string("expensive result");
		});

	CHECK(calls == 0); // construction must not invoke

	CHECK(lz.get() == "expensive result");
	CHECK(calls == 1);

	// every further access uses the cached value
	CHECK(lz.get() == "expensive result");
	CHECK(lz() == "expensive result");
	const std::string& converted = lz;
	CHECK(converted == "expensive result");
	CHECK(calls == 1);

	// accessors return references to the same cached object
	CHECK(&lz.get() == &lz());
	CHECK(&converted == &lz.get());
}

TEST_CASE("lazy - move-only results are supported", "[data][lazy]")
{
	const mh::lazy lz([] { return std::make_unique<int>(5); });

	REQUIRE(lz.get() != nullptr);
	CHECK(*lz.get() == 5);
	CHECK(lz.get() == lz.get()); // cached: same pointer every time
}

TEST_CASE("lazy - a throwing function is retried on the next access", "[data][lazy]")
{
	int calls = 0;
	const mh::lazy lz([&calls]
		{
			if (++calls == 1)
				throw std::runtime_error("first call fails");

			return calls;
		});

	CHECK_THROWS_AS(lz.get(), std::runtime_error);
	CHECK(calls == 1);

	// the failure did not consume the function: the next access retries
	CHECK(lz.get() == 2);
	CHECK(calls == 2);

	// ...and the successful result is cached
	CHECK(lz.get() == 2);
	CHECK(calls == 2);
}
