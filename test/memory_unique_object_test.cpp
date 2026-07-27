#include "mh/memory/unique_object.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <memory>
#include <utility>
#include "last_include.hpp"

// Test traits for managing an integer resource
struct IntTraits 
{
	static constexpr int invalid() { return -1; }
	void delete_obj(int& obj) { obj = -1; } // Mark as "deleted"
	int release_obj(int& obj) { 
		int temp = obj; 
		obj = -1; // Mark as released
		return temp; 
	}
	bool is_obj_valid(const int& obj) const { return obj >= 0; }
};

// Test traits for managing a pointer resource
struct PtrTraits 
{
	static constexpr int* invalid() { return nullptr; }
	void delete_obj(int*& ptr) { 
		delete ptr; 
		ptr = nullptr; 
	}
	int* release_obj(int*& ptr) { 
		int* temp = ptr; 
		ptr = nullptr; 
		return temp; 
	}
	bool is_obj_valid(const int* ptr) const { return ptr != nullptr; }
};

TEST_CASE("unique_object basic construction", "[memory][unique_object]")
{
	SECTION("default construction")
	{
		mh::unique_object<int, IntTraits> obj;
		REQUIRE(!obj); // Should be invalid by default (-1 is not valid per IntTraits)
		REQUIRE(obj.value() == -1);
	}

	SECTION("construction with value and traits")
	{
		IntTraits traits;
		mh::unique_object<int, IntTraits> obj(42, traits);
		REQUIRE(obj);
		REQUIRE(obj.value() == 42);
	}

	SECTION("construction with rvalue value")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		REQUIRE(obj.value() == 42);
	}

	SECTION("construction with pointer")
	{
		int* ptr = new int(100);
		mh::unique_object<int*, PtrTraits> obj(ptr);
		REQUIRE(obj);
		REQUIRE(obj.value() == ptr);
		REQUIRE(*obj.value() == 100);
	}
}

TEST_CASE("unique_object move semantics", "[memory][unique_object]")
{
	SECTION("move construction")
	{
		mh::unique_object<int, IntTraits> obj1(42);
		REQUIRE(obj1);
		REQUIRE(obj1.value() == 42);
		
		mh::unique_object<int, IntTraits> obj2(std::move(obj1));
		REQUIRE(obj2);
		REQUIRE(obj2.value() == 42);
		REQUIRE(!obj1); // obj1 should be invalid after move
		REQUIRE(obj1.value() == -1); // Released/deleted
	}

	SECTION("move assignment")
	{
		mh::unique_object<int, IntTraits> obj1(42);
		mh::unique_object<int, IntTraits> obj2(100);
		
		REQUIRE(obj1.value() == 42);
		REQUIRE(obj2.value() == 100);
		
		obj2 = std::move(obj1);
		
		REQUIRE(obj2);
		REQUIRE(obj2.value() == 42);
		REQUIRE(!obj1); // obj1 should be invalid after move
		REQUIRE(obj1.value() == -1); // Released/deleted
	}
}

TEST_CASE("unique_object copy semantics disabled", "[memory][unique_object]")
{
	// These should not compile (testing at compile time)
	static_assert(!std::is_copy_constructible_v<mh::unique_object<int, IntTraits>>);
	static_assert(!std::is_copy_assignable_v<mh::unique_object<int, IntTraits>>);
}

TEST_CASE("unique_object release functionality", "[memory][unique_object]")
{
	SECTION("release returns value and invalidates object")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		REQUIRE(obj.value() == 42);
		
		int released = obj.release();
		REQUIRE(released == 42);
		REQUIRE(!obj); // Should be invalid after release
		REQUIRE(obj.value() == -1); // Should be marked as released
	}

	SECTION("release with pointer")
	{
		int* ptr = new int(200);
		mh::unique_object<int*, PtrTraits> obj(ptr);
		REQUIRE(obj);
		
		int* released = obj.release();
		REQUIRE(released == ptr);
		REQUIRE(*released == 200);
		REQUIRE(!obj); // Should be invalid after release
		REQUIRE(obj.value() == nullptr);
		
		delete released; // Clean up manually
	}
}

TEST_CASE("unique_object reset functionality", "[memory][unique_object]")
{
	SECTION("reset without argument")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		
		obj.reset();
		REQUIRE(!obj);
		REQUIRE(obj.value() == -1); // Should be deleted
	}

	SECTION("reset with new value")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		REQUIRE(obj.value() == 42);
		
		obj.reset(100);
		REQUIRE(obj);
		REQUIRE(obj.value() == 100);
	}

	SECTION("reset_and_get_ref")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		
		int& ref = obj.reset_and_get_ref();
		REQUIRE(!obj); // Should be invalid after reset
		REQUIRE(obj.value() == -1); // Should be deleted
		REQUIRE(&ref == &obj.value()); // Should be reference to internal value
	}
}

TEST_CASE("unique_object boolean conversion", "[memory][unique_object]")
{
	SECTION("valid object converts to true")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		REQUIRE(static_cast<bool>(obj) == true);
	}

	SECTION("invalid object converts to false")
	{
		mh::unique_object<int, IntTraits> obj(-1); // -1 is invalid per IntTraits
		REQUIRE(!obj);
		REQUIRE(static_cast<bool>(obj) == false);
	}

	SECTION("released object converts to false")
	{
		mh::unique_object<int, IntTraits> obj(42);
		REQUIRE(obj);
		
		obj.release();
		REQUIRE(!obj);
		REQUIRE(static_cast<bool>(obj) == false);
	}
}

TEST_CASE("unique_object value access", "[memory][unique_object]")
{
	SECTION("const value access")
	{
		mh::unique_object<int, IntTraits> obj(42);
		const auto& const_obj = obj;
		
		REQUIRE(const_obj.value() == 42);
		REQUIRE(static_cast<const int&>(const_obj) == 42);
	}

	SECTION("implicit conversion to T")
	{
		mh::unique_object<int, IntTraits> obj(42);
		int value = obj; // Implicit conversion
		REQUIRE(value == 42);
	}
}

TEST_CASE("unique_object stream insertion", "[memory][unique_object]")
{
	SECTION("valid object stream insertion")
	{
		mh::unique_object<int, IntTraits> obj(42);
		std::ostringstream oss;
		oss << obj;
		REQUIRE(oss.str() == "42");
	}

	SECTION("invalid object stream insertion")
	{
		mh::unique_object<int, IntTraits> obj(-1); // Invalid
		std::ostringstream oss;
		oss << obj;
		REQUIRE(oss.str() == "(empty)");
	}

	SECTION("released object stream insertion")
	{
		mh::unique_object<int, IntTraits> obj(42);
		obj.release();
		std::ostringstream oss;
		oss << obj;
		REQUIRE(oss.str() == "(empty)");
	}
}

TEST_CASE("unique_object RAII behavior", "[memory][unique_object]")
{
	SECTION("destructor calls delete_obj")
	{
		int* ptr = new int(300);
		{
			mh::unique_object<int*, PtrTraits> obj(ptr);
			REQUIRE(obj);
			REQUIRE(*obj.value() == 300);
		} // Destructor should delete the pointer
		
		// Note: ptr is now deleted, we can't safely access it
		// The test here is that no memory leak occurs
	}

	SECTION("reset calls delete_obj on previous value")
	{
		int* ptr1 = new int(100);
		int* ptr2 = new int(200);
		
		mh::unique_object<int*, PtrTraits> obj(ptr1);
		REQUIRE(*obj.value() == 100);
		
		obj.reset(ptr2); // Should delete ptr1
		REQUIRE(*obj.value() == 200);
		
		// obj destructor will delete ptr2
	}
}

// Test with a custom stateful traits type
struct StatefulTraits 
{
	mutable int delete_count = 0;
	mutable int release_count = 0;
	
	static constexpr int invalid() { return -1; }
	void delete_obj(int& obj) const { 
		if (obj >= 0) {
			++delete_count;
			obj = -1;
		}
	}
	int release_obj(int& obj) const { 
		++release_count;
		int temp = obj; 
		obj = -1; 
		return temp; 
	}
	bool is_obj_valid(const int& obj) const { return obj >= 0; }
};

TEST_CASE("unique_object with stateful traits", "[memory][unique_object]")
{
	SECTION("traits methods are called correctly")
	{
		// Test that release makes object invalid and destructor doesn't double-delete
		{
			mh::unique_object<int, StatefulTraits> obj(42);
			REQUIRE(obj);
			REQUIRE(obj.value() == 42);
			
			int released_value = obj.release();
			REQUIRE(released_value == 42);
			REQUIRE(!obj); // Should be invalid after release
			REQUIRE(obj.value() == -1); // Should be marked as released
		} // Destructor should not cause issues for already-released object
	}
}

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
