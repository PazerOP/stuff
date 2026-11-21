#include "mh/memory/unique_object.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <utility>

namespace
{
	// Mirrors the shape of an fd-style traits type (mh::io's fd_traits), but
	// counts "closes" instead of touching real file descriptors, so lifetime
	// bugs (double close, closing handle 0) show up as assertion failures.
	struct counting_handle_traits
	{
		static constexpr int invalid() { return -1; }

		inline static int s_CloseCount = 0;

		void delete_obj(int handle) const
		{
			if (handle >= 0)
				++s_CloseCount;
		}
		int release_obj(int& handle) const
		{
			return std::exchange(handle, invalid());
		}
		bool is_obj_valid(int handle) const { return handle >= 0; }
	};

	using counting_handle = mh::unique_object<int, counting_handle_traits>;

	// Records copies/moves so we can assert which constructor was used.
	struct move_probe
	{
		inline static int s_Copies = 0;
		inline static int s_Moves = 0;

		move_probe() = default;
		move_probe(const move_probe&) { ++s_Copies; }
		move_probe(move_probe&&) noexcept { ++s_Moves; }
		move_probe& operator=(const move_probe&)
		{
			++s_Copies;
			return *this;
		}
		move_probe& operator=(move_probe&&) noexcept
		{
			++s_Moves;
			return *this;
		}
	};
	struct move_probe_traits
	{
		void delete_obj(move_probe&) const {}
		move_probe release_obj(move_probe& p) const { return std::move(p); }
		bool is_obj_valid(const move_probe&) const { return true; }
	};

	// Holding a move-only object must work too.
	struct move_only
	{
		move_only() = default;
		move_only(move_only&&) noexcept = default;
		move_only& operator=(move_only&&) noexcept = default;
	};
	struct move_only_traits
	{
		void delete_obj(move_only&) const {}
		move_only release_obj(move_only& o) const { return std::move(o); }
		bool is_obj_valid(const move_only&) const { return true; }
	};
}

TEST_CASE("unique_object - default-constructed handle is invalid and never deleted", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;
	{
		counting_handle handle;
		// must use the traits' invalid value, not a value-initialized handle
		// (0 would be a perfectly valid fd-style handle)
		REQUIRE(handle.value() == -1);
		REQUIRE(!handle);
	}
	REQUIRE(counting_handle_traits::s_CloseCount == 0);
}

TEST_CASE("unique_object - destructor deletes a held handle exactly once", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;
	{
		counting_handle handle(42);
		REQUIRE(handle);
		REQUIRE(handle.value() == 42);
	}
	REQUIRE(counting_handle_traits::s_CloseCount == 1);
}

TEST_CASE("unique_object - reset() deletes exactly once and invalidates", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;
	{
		counting_handle handle(42);

		handle.reset();
		REQUIRE(counting_handle_traits::s_CloseCount == 1);
		REQUIRE(!handle);
		REQUIRE(handle.value() == -1);
	}
	// the destructor must not delete the already-reset handle a second time
	REQUIRE(counting_handle_traits::s_CloseCount == 1);
}

TEST_CASE("unique_object - reset_and_get_ref() hands out an invalidated slot", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;
	{
		counting_handle handle(7);

		int& slot = handle.reset_and_get_ref();
		REQUIRE(counting_handle_traits::s_CloseCount == 1);
		REQUIRE(slot == -1);

		slot = 9; // caller fills in a new handle
		REQUIRE(handle.value() == 9);
		REQUIRE(handle);
	}
	REQUIRE(counting_handle_traits::s_CloseCount == 2); // only the new handle was deleted
}

TEST_CASE("unique_object - move transfers ownership without extra deletes", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;
	{
		counting_handle source(5);
		counting_handle target(std::move(source));

		REQUIRE(!source);
		REQUIRE(target);
		REQUIRE(target.value() == 5);
		REQUIRE(counting_handle_traits::s_CloseCount == 0);
	}
	REQUIRE(counting_handle_traits::s_CloseCount == 1);
}

TEST_CASE("unique_object - release() gives up ownership", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;
	{
		counting_handle handle(6);
		REQUIRE(handle.release() == 6);
		REQUIRE(!handle);
	}
	REQUIRE(counting_handle_traits::s_CloseCount == 0);
}

TEST_CASE("unique_object - reset(rvalue) moves instead of copying", "[memory][unique_object]")
{
	mh::unique_object<move_probe, move_probe_traits> holder{ move_probe{} };

	move_probe::s_Copies = 0;
	move_probe::s_Moves = 0;
	holder.reset(move_probe{});

	REQUIRE(move_probe::s_Copies == 0);
	REQUIRE(move_probe::s_Moves > 0);
}

TEST_CASE("unique_object - construction from an rvalue moves instead of copying", "[memory][unique_object]")
{
	move_probe::s_Copies = 0;
	move_probe::s_Moves = 0;
	mh::unique_object<move_probe, move_probe_traits> holder{ move_probe{} };

	REQUIRE(move_probe::s_Copies == 0);
	REQUIRE(move_probe::s_Moves > 0);
}

TEST_CASE("unique_object - supports move-only object types", "[memory][unique_object]")
{
	// merely compiling this proves the single-argument rvalue constructor path
	// exists (a copy-only overload set could not hold a move-only type)
	mh::unique_object<move_only, move_only_traits> holder{ move_only{} };
	holder.reset(move_only{});
	REQUIRE(static_cast<bool>(holder));
}

TEST_CASE("unique_object - stream insertion prints the value or (empty)", "[memory][unique_object]")
{
	counting_handle_traits::s_CloseCount = 0;

	{
		std::ostringstream os;
		os << counting_handle(42);
		REQUIRE(os.str() == "42");
	}

	{
		std::ostringstream os;
		os << counting_handle(); // invalid handle
		REQUIRE(os.str() == "(empty)");
	}
}
