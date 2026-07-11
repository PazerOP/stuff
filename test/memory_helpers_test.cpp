#include "mh/memory/memory_helpers.hpp"
#include <catch2/catch_all.hpp>

#include <memory>

namespace
{
	struct two_members
	{
		int m_First = 1;
		int m_Second = 2;
	};
}

TEST_CASE("shared_ptr_to_member - aliases the member the caller asked for", "[memory][memory_helpers]")
{
	auto owner = std::make_shared<two_members>();

	std::shared_ptr<int> second = mh::shared_ptr_to_member(owner, &two_members::m_Second);
	REQUIRE(second.get() == &owner->m_Second);
	REQUIRE(*second == 2);

	std::shared_ptr<int> first = mh::shared_ptr_to_member(owner, &two_members::m_First);
	REQUIRE(first.get() == &owner->m_First);
	REQUIRE(*first == 1);

	// all three share the owner's control block
	REQUIRE(owner.use_count() == 3);

	// writes through the alias hit the owning object
	*second = 42;
	REQUIRE(owner->m_Second == 42);
	REQUIRE(owner->m_First == 1);
}

TEST_CASE("shared_ptr_to_member - keeps the owning object alive", "[memory][memory_helpers]")
{
	auto owner = std::make_shared<two_members>();
	std::weak_ptr<two_members> watcher = owner;

	std::shared_ptr<int> member = mh::shared_ptr_to_member(owner, &two_members::m_Second);

	owner.reset();
	REQUIRE(!watcher.expired()); // the member alias keeps the whole object alive
	REQUIRE(*member == 2);

	member.reset();
	REQUIRE(watcher.expired());
}
