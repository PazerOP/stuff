// NOTE: deliberately NOT a networked test. mh::http::get performs a real HTTP
// request; CI must not depend on network reachability, so this only checks the
// API surface (and, in compiled-library mode, that the symbol actually links).
#include "mh/http/client.hpp"

#include <catch2/catch_all.hpp>

#include <string>
#include <type_traits>

TEST_CASE("http client - API surface", "[http]")
{
	// regression (compile-time): get() must take its URL BY VALUE. The coroutine
	// hops threads and evaluates the url afterwards; a const std::string&
	// parameter dangled for any temporary argument.
	using get_ptr_t = mh::task<mh::http::response> (*)(std::string);
	STATIC_CHECK(std::is_same_v<decltype(&mh::http::get), get_ptr_t>);

	STATIC_CHECK(std::is_same_v<decltype(mh::http::get(std::declval<std::string>())),
		mh::task<mh::http::response>>);

	// response is a value aggregate
	mh::http::response resp;
	resp.status.value = 200;
	resp.headers["Content-Type"] = "text/plain";
	resp.body = "ok";
	CHECK(resp.status.value == 200);
	CHECK(resp.body == "ok");

#ifdef MH_COMPILE_LIBRARY
	// In compiled mode the definition lives in the library: odr-use the function
	// so a missing/mis-declared symbol becomes a link error here.
	auto* fn = &mh::http::get;
	CHECK(fn != nullptr);
#endif
}
