#include "mh/memory/unique_object.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <memory>

// Test traits for managing an integer resource
struct IntTraits 
{
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
		REQUIRE(!obj); // Should be invalid by default (0 is not valid per IntTraits)
		REQUIRE(obj.value() == 0);
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
		mh::unique_object<int, IntTraits> obj(0); // 0 is invalid per IntTraits
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
		mh::unique_object<int, IntTraits> obj(0); // Invalid
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
		StatefulTraits traits;
		{
			mh::unique_object<int, StatefulTraits> obj(42, traits);
			REQUIRE(obj);
			REQUIRE(traits.delete_count == 0);
			REQUIRE(traits.release_count == 0);
			
			obj.release();
			REQUIRE(traits.release_count == 1);
			REQUIRE(traits.delete_count == 0);
		} // Destructor should call delete_obj, but object is already released
		
		REQUIRE(traits.release_count == 1);
		REQUIRE(traits.delete_count == 0); // delete_obj should not increment for invalid objects
	}
}