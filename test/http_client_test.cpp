// NOTE: deliberately NOT an external-network test. CI must not depend on
// network reachability, so the runtime tests below talk exclusively to an
// in-process loopback (127.0.0.1) server on an ephemeral port; everything else
// only checks the API surface (and, in compiled-library mode, that the symbol
// actually links).
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

// The runtime tests need the compiled-library definitions (cpp/src only builds
// in MH_COMPILE_LIBRARY mode) and a POSIX socket API for the loopback server.
#if defined(MH_COMPILE_LIBRARY) && defined(__unix__)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace mh::http
{
	// Defined (with external linkage) in cpp/src/http/client.cpp but never
	// declared in a header; declare it here so the compiled-library symbol can
	// be exercised directly.
	response get_sync(const std::string& url);
}

namespace
{
	// If the environment routes http:// through a proxy (http_proxy) without a
	// no_proxy exemption, curl would send our 127.0.0.1 requests to the proxy
	// instead of the in-process server. Set pre-main (single-threaded) and only
	// when absent (overwrite=0) so an existing configuration is never touched.
	[[maybe_unused]] const int g_no_proxy_env = (::setenv("no_proxy", "127.0.0.1", 0), 0);

	[[noreturn]] void throw_errno(const char* what)
	{
		throw std::system_error(errno, std::generic_category(), what);
	}

	// Minimal HTTP/1.1 server on 127.0.0.1:<ephemeral>. Serves one canned
	// response per accepted connection, in order, then exits its thread. Every
	// blocking operation is a 100ms poll slice bounded by a 30s deadline and a
	// stop flag, so no test can hang CI: destruction wakes the thread within
	// ~100ms even mid-wait.
	class loopback_http_server
	{
	public:
		explicit loopback_http_server(std::vector<std::string> responses) :
			m_Responses(std::move(responses))
		{
			m_ListenFD = ::socket(AF_INET, SOCK_STREAM, 0);
			if (m_ListenFD < 0)
				throw_errno("socket");

			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			addr.sin_port = 0; // ephemeral: the OS picks a free port

			if (::bind(m_ListenFD, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0 ||
				::listen(m_ListenFD, 8) != 0)
			{
				throw_errno("bind/listen");
			}

			sockaddr_in bound{};
			socklen_t boundSize = sizeof(bound);
			if (::getsockname(m_ListenFD, reinterpret_cast<sockaddr*>(&bound), &boundSize) != 0)
				throw_errno("getsockname");

			m_Port = ntohs(bound.sin_port);
			m_Thread = std::thread(&loopback_http_server::serve, this);
		}

		loopback_http_server(const loopback_http_server&) = delete;
		loopback_http_server& operator=(const loopback_http_server&) = delete;

		~loopback_http_server()
		{
			m_Stop = true;
			if (m_Thread.joinable())
				m_Thread.join();

			::close(m_ListenFD);
		}

		uint16_t port() const { return m_Port; }

		std::string url(const std::string& path) const
		{
			return "http://127.0.0.1:" + std::to_string(m_Port) + path;
		}

		// Raw request bytes, one entry per connection served so far. Only assert
		// on this from the main thread after the corresponding client call
		// returned (the request is recorded before its response is sent).
		std::vector<std::string> requests() const
		{
			std::lock_guard lock(m_Mutex);
			return m_Requests;
		}

	private:
		// True once fd is readable (or in a readiness-error state, which the
		// following accept/recv will surface). False on stop/timeout.
		bool wait_readable(int fd) const
		{
			using clock = std::chrono::steady_clock;
			const auto deadline = clock::now() + std::chrono::seconds(30);
			while (!m_Stop && clock::now() < deadline)
			{
				pollfd pfd{};
				pfd.fd = fd;
				pfd.events = POLLIN;
				if (::poll(&pfd, 1, 100) > 0)
					return true;
			}

			return false;
		}

		void serve()
		{
			for (const std::string& response : m_Responses)
			{
				if (!wait_readable(m_ListenFD))
					return;

				const int client = ::accept(m_ListenFD, nullptr, nullptr);
				if (client < 0)
					return;

				handle_connection(client, response);
				::close(client);
			}
		}

		void handle_connection(int client, const std::string& response)
		{
			// A GET request has no body: read until the end-of-headers marker.
			std::string request;
			while (!m_Stop && request.find("\r\n\r\n") == std::string::npos && request.size() < 65536)
			{
				if (!wait_readable(client))
					break;

				char buf[4096];
				const ssize_t got = ::recv(client, buf, sizeof(buf), 0);
				if (got <= 0)
					break;

				request.append(buf, static_cast<size_t>(got));
			}

			{
				std::lock_guard lock(m_Mutex);
				m_Requests.push_back(std::move(request));
			}

			size_t sent = 0;
			while (!m_Stop && sent < response.size())
			{
				// MSG_NOSIGNAL: a peer that already closed must produce EPIPE, not
				// a process-killing SIGPIPE.
				const ssize_t wrote = ::send(client, response.data() + sent, response.size() - sent, MSG_NOSIGNAL);
				if (wrote <= 0)
					break;

				sent += static_cast<size_t>(wrote);
			}
		}

		std::vector<std::string> m_Responses;
		std::vector<std::string> m_Requests;
		mutable std::mutex m_Mutex;
		std::atomic_bool m_Stop = false;
		int m_ListenFD = -1;
		uint16_t m_Port = 0;
		std::thread m_Thread;
	};

	// "Connection: close" keeps the connection<->response mapping 1:1 (curl
	// reconnects for a followed redirect instead of reusing the connection).
	std::string ok_response(const std::string& body, const std::string& extraHeaders = {})
	{
		return
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: " + std::to_string(body.size()) + "\r\n"
			+ extraHeaders +
			"Connection: close\r\n"
			"\r\n"
			+ body;
	}
}

TEST_CASE("http get_sync - loopback success", "[http]")
{
	loopback_http_server server({
		ok_response("hello loopback",
			"Content-Type: text/plain\r\n"
			"X-Padded:  \t needs trimming \t \r\n"
			"X-Tight:tight\r\n"
			"X-Empty:\r\n"),
	});

	const mh::http::response resp = mh::http::get_sync(server.url("/hello"));

	CHECK(resp.status.value == 200);
	CHECK(resp.status.is_success());
	CHECK(resp.body == "hello loopback");

	// header_callback trims leading/trailing whitespace (and the line's \r\n)
	// from values; the status line and the blank line take its no-colon path.
	REQUIRE(resp.headers.contains("Content-Type"));
	CHECK(resp.headers.at("Content-Type") == "text/plain");
	REQUIRE(resp.headers.contains("X-Padded"));
	CHECK(resp.headers.at("X-Padded") == "needs trimming");
	REQUIRE(resp.headers.contains("X-Tight"));
	CHECK(resp.headers.at("X-Tight") == "tight");
	REQUIRE(resp.headers.contains("X-Empty"));
	CHECK(resp.headers.at("X-Empty") == "");

	// The client really spoke HTTP/1.1 to our socket
	const auto requests = server.requests();
	REQUIRE(requests.size() == 1);
	CHECK(requests[0].starts_with("GET /hello HTTP/1.1\r\n"));
	CHECK(requests[0].find("Host: 127.0.0.1:") != std::string::npos);
}

TEST_CASE("http get_sync - follows redirects", "[http]")
{
	loopback_http_server server({
		"HTTP/1.1 301 Moved Permanently\r\n"
		"Location: /target\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n",
		ok_response("after redirect"),
	});

	const mh::http::response resp = mh::http::get_sync(server.url("/moved"));

	// CURLOPT_FOLLOWLOCATION: the reported status/body are the final hop's...
	CHECK(resp.status.value == 200);
	CHECK(resp.body == "after redirect");

	// ...while the header map accumulates across hops (the 301's Location
	// header remains visible).
	REQUIRE(resp.headers.contains("Location"));
	CHECK(resp.headers.at("Location") == "/target");

	const auto requests = server.requests();
	REQUIRE(requests.size() == 2);
	CHECK(requests[0].starts_with("GET /moved HTTP/1.1\r\n"));
	CHECK(requests[1].starts_with("GET /target HTTP/1.1\r\n"));
}

TEST_CASE("http get_sync - connection refused", "[http]")
{
	// Grab an ephemeral port that nothing is listening on: bind + close.
	uint16_t port = 0;
	{
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(fd >= 0);

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		REQUIRE(::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0);

		socklen_t addrSize = sizeof(addr);
		REQUIRE(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addrSize) == 0);
		port = ntohs(addr.sin_port);
		::close(fd);
	}

	const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/nobody-home";
	REQUIRE_THROWS_WITH(mh::http::get_sync(url),
		Catch::Matchers::StartsWith("HTTP request failed: "));
	REQUIRE_THROWS_AS(mh::http::get_sync(url), std::runtime_error);
}

TEST_CASE("http get_sync - truncated response", "[http]")
{
	// Content-Length promises more than the server delivers before closing:
	// curl reports the transfer as failed, which get_sync turns into a throw.
	loopback_http_server server({
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 1000\r\n"
		"Connection: close\r\n"
		"\r\n"
		"only this much",
	});

	REQUIRE_THROWS_WITH(mh::http::get_sync(server.url("/truncated")),
		Catch::Matchers::StartsWith("HTTP request failed: "));
}

TEST_CASE("http get - coroutine success", "[http]")
{
	loopback_http_server server({
		ok_response("from the coroutine", "X-Coro:  spaced  \r\n"),
	});

	// get() hops to its own thread and runs the same curl request there;
	// task::get() blocks until it completes.
	mh::task<mh::http::response> task = mh::http::get(server.url("/coro"));
	const mh::http::response& resp = task.get();

	CHECK(resp.status.value == 200);
	CHECK(resp.body == "from the coroutine");
	REQUIRE(resp.headers.contains("X-Coro"));
	CHECK(resp.headers.at("X-Coro") == "spaced");

	const auto requests = server.requests();
	REQUIRE(requests.size() == 1);
	CHECK(requests[0].starts_with("GET /coro HTTP/1.1\r\n"));
}

TEST_CASE("http get - coroutine error propagation", "[http]")
{
	// Same refused-connection setup as the get_sync test, but the throw happens
	// on the coroutine's thread and must surface through the task.
	uint16_t port = 0;
	{
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		REQUIRE(fd >= 0);

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		REQUIRE(::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0);

		socklen_t addrSize = sizeof(addr);
		REQUIRE(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addrSize) == 0);
		port = ntohs(addr.sin_port);
		::close(fd);
	}

	mh::task<mh::http::response> task =
		mh::http::get("http://127.0.0.1:" + std::to_string(port) + "/nobody-home");
	REQUIRE_THROWS_WITH(task.get(), Catch::Matchers::StartsWith("HTTP request failed: "));
}

#endif // defined(MH_COMPILE_LIBRARY) && defined(__unix__)
