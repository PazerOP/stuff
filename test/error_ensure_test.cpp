// The header must be usable standalone: it has to provide its own MH_STUFF_API
// fallback even when the build system's definition is not in effect.
#ifdef MH_STUFF_API
#undef MH_STUFF_API
#endif
#include "mh/error/ensure.hpp"

#include <catch2/catch_all.hpp>

#include <memory>
#include <typeinfo> // ensure_traits_default prints typeid names for non-streamable types
#include <utility>

TEST_CASE("ensure - debug break helper is declared by the header", "[error][ensure]")
{
	// MH_ERROR_ENSURE_HPP_DEBUGBREAK() expands to raise(SIGTRAP) on POSIX; the
	// header must pull in the declarations it uses. Compiling this
	// never-invoked lambda is the test.
	auto neverCalled = [] { MH_ERROR_ENSURE_HPP_DEBUGBREAK(); };
	(void)neverCalled;
	SUCCEED("header provides the declarations used by its own macros");
}

TEST_CASE("ensure - release-mode mh_ensure passes lvalues through untouched", "[error][ensure]")
{
#ifndef _DEBUG
	auto original = std::make_shared<int>(42);

	auto copy = mh_ensure(original);

	REQUIRE(original != nullptr);
	REQUIRE(copy.get() == original.get());
	REQUIRE(original.use_count() == 2);
	REQUIRE(*mh_ensure(original) == 42);
#else
	SUCCEED("release-only check skipped in _DEBUG builds");
#endif
}

TEST_CASE("ensure - checked implementation does not steal from lvalue arguments", "[error][ensure]")
{
	// mh_ensure_impl is what mh_ensure expands to in _DEBUG builds; it must
	// return lvalue arguments by forwarding (copy), not by moving from them.
	auto original = std::make_shared<int>(42);

#if defined(__GNUC__)
	// The designated initializer inside the macro leaves the base subobject
	// without an explicit initializer; that pre-existing -Wextra warning is
	// unrelated to the forwarding behavior verified here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
	auto copy = mh_ensure_impl(original, ::mh::ensure_traits, nullptr);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	REQUIRE(original != nullptr); // must not be moved-from
	REQUIRE(copy.get() == original.get());
	REQUIRE(original.use_count() == 2);
}

TEST_CASE("ensure - checked implementation still moves rvalue arguments", "[error][ensure]")
{
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
	auto result = mh_ensure_impl(std::make_shared<int>(7), ::mh::ensure_traits, nullptr);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	REQUIRE(result != nullptr);
	REQUIRE(*result == 7);
	REQUIRE(result.use_count() == 1);
}
