// The header must be usable standalone: it has to provide its own MH_STUFF_API
// fallback even when the build system's definition is not in effect.
#ifdef MH_STUFF_API
#undef MH_STUFF_API
#endif
#include "mh/error/ensure.hpp"

#include <catch2/catch_all.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo> // ensure_traits_default prints typeid names for non-streamable types
#include <utility>

namespace
{
	// Redirects std::cerr into a string for the duration of its scope; the
	// failure path of mh_ensure reports through std::cerr.
	class cerr_capture final
	{
	public:
		cerr_capture() : m_OldBuf(std::cerr.rdbuf(m_Stream.rdbuf())) {}
		~cerr_capture() { std::cerr.rdbuf(m_OldBuf); }

		cerr_capture(const cerr_capture&) = delete;
		cerr_capture& operator=(const cerr_capture&) = delete;

		std::string str() const { return m_Stream.str(); }

	private:
		std::ostringstream m_Stream;
		std::streambuf* m_OldBuf;
	};

	struct not_streamable
	{
		constexpr bool operator!() const { return true; } // always triggers
	};

	// Forces the value to be printed even though not_streamable has no
	// operator<<: exercises the '{typeid-name}' fallback of print_value().
	template<typename T>
	struct force_print_traits : mh::ensure_traits_default<T>
	{
	protected:
		bool can_print_value() const override { return true; }
	};
}

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

TEST_CASE("ensure - passing check prints nothing", "[error][ensure]")
{
	cerr_capture capture;

	int truthy = 3;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
	const auto result = mh_ensure_impl(truthy, ::mh::ensure_traits, "never printed");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	CHECK(result == 3);
	CHECK(capture.str().empty());
}

TEST_CASE("ensure - failing check reports to stderr and passes the value through", "[error][ensure]")
{
	cerr_capture capture;

	int zero = 0;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
	const auto result = mh_ensure_impl(zero, ::mh::ensure_traits, nullptr);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	// the failure is reported, not thrown: the value still passes through
	CHECK(result == 0);

	const std::string output = capture.str();
	CHECK(output.find("mh_ensure failed: zero") != std::string::npos);

	// details labels are right-justified to the width of "message" (7)
	CHECK(output.find("\n\t     in :  ") != std::string::npos);
	CHECK(output.find("\n\t     at :  ") != std::string::npos);
	CHECK(output.find("error_ensure_test") != std::string::npos); // file name in the "at" line

	// no message was supplied, and a streamable value gets printed
	CHECK(output.find("message :  ") == std::string::npos);
	CHECK(output.find("\n\t  value :  0") != std::string::npos);
}

TEST_CASE("ensure - failing check includes a non-empty message", "[error][ensure]")
{
	cerr_capture capture;

	int zero = 0;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
	(void)mh_ensure_impl(zero, ::mh::ensure_traits, "the failure explanation");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	CHECK(capture.str().find("\n\tmessage :  the failure explanation") != std::string::npos);
}

TEST_CASE("ensure - empty message is treated as no message", "[error][ensure]")
{
	cerr_capture capture;

	int zero = 0;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
	(void)mh_ensure_impl(zero, ::mh::ensure_traits, "");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	const std::string output = capture.str();
	CHECK(output.find("mh_ensure failed: zero") != std::string::npos);
	CHECK(output.find("message :  ") == std::string::npos);
}

TEST_CASE("ensure - non-streamable value prints its type name when forced", "[error][ensure]")
{
#if __cpp_concepts >= 201907
	// default traits cannot stream not_streamable -> no value line at all
	{
		cerr_capture capture;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
		(void)mh_ensure_impl(not_streamable{}, ::mh::ensure_traits, nullptr);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
		const std::string output = capture.str();
		CHECK(output.find("mh_ensure failed:") != std::string::npos);
		CHECK(output.find("value :  ") == std::string::npos);
	}
#endif

	// traits that force printing fall back to '{typeid-name}'
	{
		cerr_capture capture;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
		(void)mh_ensure_impl(not_streamable{}, force_print_traits, nullptr);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
		const std::string expected = std::string("value :  {") + typeid(not_streamable).name() + '}';
		CHECK(capture.str().find(expected) != std::string::npos);
	}
}
