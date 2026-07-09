#include "mh/future.hpp"
#include "mh/concurrency/async.hpp"
#include "mh/coroutine/future.hpp"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

TEST_CASE("make_ready_future - accepts lvalues, const lvalues, and rvalues")
{
	// regression: the forwarding-reference overload used to win for non-const
	// lvalues, deduce T = int&, and fail to compile. Passing a plain variable is
	// the most common call shape, so it must keep compiling.
	int x = 5;
	auto f1 = mh::make_ready_future(x);
	REQUIRE(f1.get() == 5);

	std::string s = "a string that is long enough to defeat any small string optimization";
	auto f2 = mh::make_ready_future(s);
	REQUIRE(f2.get() == s); // the lvalue argument is copied, not moved from

	const std::string cs = "const lvalue";
	auto f3 = mh::make_ready_future(cs);
	REQUIRE(f3.get() == cs);

	auto f4 = mh::make_ready_future(std::string("rvalue"));
	REQUIRE(f4.get() == "rvalue");

	auto f5 = mh::make_ready_future<int>();
	REQUIRE(f5.get() == 0);

	auto f6 = mh::emplace_ready_future<std::string>(std::size_t(3), 'x');
	REQUIRE(f6.get() == "xxx");

	auto f7 = mh::make_ready_future(1);
	REQUIRE(mh::is_future_ready(f7));
}

TEST_CASE("make_failed_future - delivers the exception")
{
	auto f = mh::make_failed_future<int>(std::runtime_error("expected failure"));
	REQUIRE(f.valid());
	REQUIRE_THROWS_AS(f.get(), std::runtime_error);
}

TEST_CASE("promise<void> - can be instantiated and completed")
{
	// regression: mh::promise<void> used to be ill-formed (its set_value had a
	// parameter of type void), so no void promise or void mh::async could compile.
	mh::promise<void> p;
	auto f = p.get_future();
	REQUIRE(f.valid());
	REQUIRE(!mh::is_future_ready(f));

	p.set_value();
	REQUIRE(mh::is_future_ready(f));
	f.wait(); // must not block or throw
}

TEST_CASE("promise/future - value and exception delivery")
{
	// NOTE: promise-backed (non-coroutine) shared states used to leak - they were
	// allocated with new but the free path was unreachable. Freeing cannot be
	// asserted from within the process; it is verified out-of-band by running this
	// test under AddressSanitizer/LeakSanitizer. This test pins down the functional
	// behavior of those same code paths.
	{
		mh::promise<int> p;
		auto f = p.get_future();

		std::thread completer([&p] { p.set_value(7); });
		f.wait();
		REQUIRE(f.get() == 7);
		completer.join();
	}

	{
		mh::promise<int> p;
		auto f = p.get_future();
		p.set_exception(std::make_exception_ptr(std::runtime_error("failure")));
		REQUIRE_THROWS_AS(f.get(), std::runtime_error);
	}

	{
		mh::promise<int> p;
		mh::shared_future<int> sf = p.get_future().share();
		mh::shared_future<int> sf2 = sf; // shared futures are copyable
		p.set_value(9);
		REQUIRE(sf.get() == 9);
		REQUIRE(sf2.get() == 9);
	}
}

TEST_CASE("async - accepts lvalue callables and arguments")
{
	// regression: mh::async used to compile only for rvalue callables with rvalue
	// arguments (unlike std::async); lvalues tripped a static_assert inside
	// std::thread.
	auto add = [](int a, int b) { return a + b; };
	int lhs = 2;
	auto f = mh::async(add, lhs, 3);
	REQUIRE(f.get() == 5);

	// void-returning callables used to be doubly broken (promise<void> + no
	// void-aware set_value call in the async worker)
	std::atomic<bool> ran = false;
	auto voidFn = [&ran] { ran = true; };
	auto fv = mh::async(voidFn);
	fv.wait();
	REQUIRE(ran);

	// rvalue callables keep working
	auto fr = mh::async([] { return std::string("done"); });
	REQUIRE(fr.get() == "done");

	// exceptions propagate through the future
	auto fe = mh::async([]() -> int { throw std::runtime_error("async failure"); });
	REQUIRE_THROWS_AS(fe.get(), std::runtime_error);
}

#ifdef MH_COROUTINES_SUPPORTED

TEST_CASE("future - moved-from futures are empty")
{
	// regression: a user-provided (empty) task destructor suppressed the move
	// operations, so mh::future's "move" silently degraded to a refcount copy and
	// the moved-from future stayed attached to the shared state.
	mh::promise<int> p;
	mh::future<int> f = p.get_future();
	p.set_value(11);

	mh::future<int> f2 = std::move(f);
	REQUIRE(f.empty()); // intentional use-after-move: this IS the regression check
	REQUIRE(!f.valid());
	REQUIRE(f2.get() == 11);

	mh::future<int> f3;
	f3 = std::move(f2);
	REQUIRE(f2.empty());
	REQUIRE(f3.get() == 11);
}

TEST_CASE("promise - get_task shares the same state as get_future")
{
	mh::promise<int> p;
	mh::task<int> t = p.get_task();
	REQUIRE(!t.is_ready());
	p.set_value(21);
	REQUIRE(t.is_ready());
	REQUIRE(t.get() == 21);
}

#endif
